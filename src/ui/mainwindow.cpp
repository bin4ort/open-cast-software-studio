// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#include "ui/mainwindow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <fstream>
#include <sstream>

#include "anonymize/redactor.h"
#include "audio/device.h"
#include "audio/fileio.h"
#include "processing/job.h"
#include "ui/livepanel.h"
#include "ui/voicepanel.h"
#include "ui/waveform.h"

namespace ocs::ui {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("OCS Studio"));
    resize(920, 640);

    auto* tabs = new QTabWidget(this);
    tabs->addTab(build_process_tab(), QStringLiteral("Process file"));
    tabs->addTab(build_live_tab(), QStringLiteral("Live masking"));
    tabs->addTab(build_transcript_tab(), QStringLiteral("Transcript"));
    setCentralWidget(tabs);

    live_engine_ = new LiveEngine;
    process_poll_timer_ = new QTimer(this);
    process_poll_timer_->setInterval(100);
    connect(process_poll_timer_, &QTimer::timeout, this, &MainWindow::on_process_poll);

    live_poll_timer_ = new QTimer(this);
    live_poll_timer_->setInterval(40);
    connect(live_poll_timer_, &QTimer::timeout, this, &MainWindow::on_live_poll);
}

MainWindow::~MainWindow() {
    if (processing_.load()) {
        processing_.store(false);
        if (process_thread_.joinable()) process_thread_.join();
    }
    if (live_engine_) {
        live_engine_->stop();
        delete live_engine_;
        live_engine_ = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Process tab
// ---------------------------------------------------------------------------
QWidget* MainWindow::build_process_tab() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    auto* file_group = new QGroupBox(QStringLiteral("Files"));
    auto* file_form = new QFormLayout(file_group);

    auto* pick_row = new QHBoxLayout;
    input_path_ = new QLineEdit;
    auto* pick = new QPushButton(QStringLiteral("Browse..."));
    connect(pick, &QPushButton::clicked, this, &MainWindow::on_pick_input);
    pick_row->addWidget(input_path_, 1);
    pick_row->addWidget(pick);
    file_form->addRow(QStringLiteral("Input:"), pick_row);

    output_path_ = new QLineEdit;
    output_path_->setPlaceholderText(QStringLiteral("(default: <input>.anon.ext)"));
    file_form->addRow(QStringLiteral("Output:"), output_path_);
    layout->addWidget(file_group);

    auto* voice_group = new QGroupBox(QStringLiteral("Voice"));
    auto* voice_layout = new QVBoxLayout(voice_group);
    voice_panel_ = new VoicePanel(false);
    voice_layout->addWidget(voice_panel_);
    layout->addWidget(voice_group);

    auto* opts_group = new QGroupBox(QStringLiteral("Options"));
    auto* opts_form = new QFormLayout(opts_group);

    mode_combo_ = new QComboBox;
    mode_combo_->addItem(QStringLiteral("Global (shift all speech)"), "global");
    mode_combo_->addItem(QStringLiteral("Selective (PII segments only)"), "selective");
    opts_form->addRow(QStringLiteral("Mode:"), mode_combo_);

    denoise_check_ = new QCheckBox(QStringLiteral("Spectral denoising"));
    transcribe_check_ = new QCheckBox(QStringLiteral("Transcribe + redact along the way"));
    opts_form->addRow(QStringLiteral(""), denoise_check_);
    opts_form->addRow(QStringLiteral(""), transcribe_check_);
    layout->addWidget(opts_group);

    auto* run_row = new QHBoxLayout;
    process_button_ = new QPushButton(QStringLiteral("Process"));
    connect(process_button_, &QPushButton::clicked, this, &MainWindow::on_process);
    status_label_ = new QLabel(QStringLiteral("Idle"));
    run_row->addWidget(process_button_);
    run_row->addWidget(status_label_, 1);
    layout->addLayout(run_row);

    auto* wave_row = new QHBoxLayout;
    input_waveform_ = new Waveform;
    output_waveform_ = new Waveform;
    wave_row->addWidget(input_waveform_);
    wave_row->addWidget(output_waveform_);
    layout->addLayout(wave_row, 1);

    return page;
}

void MainWindow::on_pick_input() {
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Open audio file"), QString(),
        QStringLiteral("Audio (*.wav *.flac *.ogg *.mp3 *.m4a *.aiff *.aif)"));
    if (!path.isEmpty()) {
        input_path_->setText(path);
        const QString out = QFileInfo(path).completeBaseName() + QStringLiteral(".anon.wav");
        if (output_path_->text().isEmpty()) output_path_->setText(out);
        std::string err;
        AudioData ad = audio::read_file(path.toStdString(), &err);
        if (!ad.samples.empty()) {
            input_waveform_->set_samples(ad.samples, ad.sample_rate);
        }
    }
}

void MainWindow::on_process() {
    if (processing_.load()) return;

    const QString in = input_path_->text();
    if (in.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("OCS Studio"),
                             QStringLiteral("Pick an input file first."));
        return;
    }

    job::JobOptions opts;
    opts.input = in.toStdString();
    opts.output = output_path_->text().isEmpty()
                      ? (QFileInfo(in).completeBaseName() + QStringLiteral(".anon.wav")).toStdString()
                      : output_path_->text().toStdString();
    opts.shift = voice_panel_->params();
    opts.shift.enable_denoise = denoise_check_->isChecked();
    opts.target_gender = voice_panel_->gender();
    opts.mode = mode_combo_->currentData().toString() == QStringLiteral("selective")
                    ? dsp::AnonMode::Selective
                    : dsp::AnonMode::Global;
    opts.transcribe = transcribe_check_->isChecked();
    opts.write_transcript = transcribe_check_->isChecked();

    processing_.store(true);
    process_done_.store(false);
    status_label_->setText(QStringLiteral("Processing..."));
    process_button_->setEnabled(false);

    process_thread_ = std::thread([this, opts] {
        const job::JobResult res = job::run(opts);
        {
            process_ok_ = res.ok;
            process_message_ = res.message;
            process_segments_ = res.transcript;
        }
        process_done_.store(true);
        processing_.store(false);
    });
    process_poll_timer_->start();
}

void MainWindow::on_process_poll() {
    if (!process_done_.load()) return;
    process_poll_timer_->stop();
    process_button_->setEnabled(true);

    if (!process_ok_) {
        status_label_->setText(QStringLiteral("Error: ") +
                               QString::fromStdString(process_message_));
        QMessageBox::critical(this, QStringLiteral("OCS Studio"),
                              QString::fromStdString(process_message_));
        return;
    }
    status_label_->setText(QStringLiteral("Done"));
    if (!process_message_.empty() && process_message_ != "ok") {
        status_label_->setText(QString::fromStdString(process_message_));
    }

    std::string err;
    AudioData ad = audio::read_file(output_path_->text().toStdString(), &err);
    if (!ad.samples.empty()) {
        output_waveform_->set_samples(ad.samples, ad.sample_rate);
    }
    if (!process_segments_.empty()) {
        transcript_edit_->clear();
        for (const auto& seg : process_segments_) {
            transcript_edit_->appendPlainText(QString::fromStdString(seg.text));
        }
    }
}

// ---------------------------------------------------------------------------
// Live tab
// ---------------------------------------------------------------------------
QWidget* MainWindow::build_live_tab() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    auto* dev_group = new QGroupBox(QStringLiteral("Devices"));
    auto* dev_form = new QFormLayout(dev_group);

    in_device_combo_ = new QComboBox;
    out_device_combo_ = new QComboBox;
    const auto inputs = audio::list_input_devices();
    const auto outputs = audio::list_output_devices();
    const int din = audio::default_input_device();
    const int dout = audio::default_output_device();
    for (const auto& d : inputs) {
        const QString label = QString::number(d.index) + QStringLiteral("  ") +
                              QString::fromStdString(d.name) +
                              (d.index == din ? QStringLiteral("  [default]") : QString());
        in_device_combo_->addItem(label, d.index);
    }
    for (const auto& d : outputs) {
        const QString label = QString::number(d.index) + QStringLiteral("  ") +
                              QString::fromStdString(d.name) +
                              (d.index == dout ? QStringLiteral("  [default]") : QString());
        out_device_combo_->addItem(label, d.index);
    }
    if (din >= 0) in_device_combo_->setCurrentIndex(
        in_device_combo_->findData(din));
    if (dout >= 0) out_device_combo_->setCurrentIndex(
        out_device_combo_->findData(dout));
    dev_form->addRow(QStringLiteral("Input (mic):"), in_device_combo_);
    dev_form->addRow(QStringLiteral("Output (speaker):"), out_device_combo_);
    layout->addWidget(dev_group);

    auto* voice_group = new QGroupBox(QStringLiteral("Voice"));
    auto* voice_layout = new QVBoxLayout(voice_group);
    live_voice_panel_ = new VoicePanel(true);
    connect(live_voice_panel_, &VoicePanel::changed, this, [this] {
        if (live_engine_ && live_engine_->running()) {
            live_engine_->set_pitch_semitones(
                live_voice_panel_->params().pitch_semitones);
        }
    });
    voice_layout->addWidget(live_voice_panel_);
    layout->addWidget(voice_group);

    auto* ctrl_group = new QGroupBox(QStringLiteral("Controls"));
    auto* ctrl_form = new QFormLayout(ctrl_group);

    sample_rate_spin_ = new QSpinBox;
    sample_rate_spin_->setRange(8000, 192000);
    sample_rate_spin_->setValue(48000);
    sample_rate_spin_->setSuffix(QStringLiteral(" Hz"));
    ctrl_form->addRow(QStringLiteral("Sample rate:"), sample_rate_spin_);

    mute_silence_check_ = new QCheckBox(QStringLiteral("Mute when not speaking"));
    dry_check_ = new QCheckBox(QStringLiteral("Dry run (no shift)"));
    ctrl_form->addRow(QStringLiteral(""), mute_silence_check_);
    ctrl_form->addRow(QStringLiteral(""), dry_check_);
    layout->addWidget(ctrl_group);

    auto* run_row = new QHBoxLayout;
    live_button_ = new QPushButton(QStringLiteral("Start"));
    connect(live_button_, &QPushButton::clicked, this, &MainWindow::on_live_start_stop);
    live_status_label_ = new QLabel(QStringLiteral("Stopped"));
    run_row->addWidget(live_button_);
    run_row->addWidget(live_status_label_, 1);
    layout->addLayout(run_row);

    live_waveform_ = new Waveform;
    layout->addWidget(live_waveform_, 1);
    return page;
}

void MainWindow::on_live_start_stop() {
    if (live_engine_->running()) {
        live_engine_->stop();
        live_button_->setText(QStringLiteral("Start"));
        live_status_label_->setText(QStringLiteral("Stopped"));
        live_poll_timer_->stop();
        return;
    }

    const int in_dev = in_device_combo_->currentData().toInt();
    const int out_dev = out_device_combo_->currentData().toInt();
    const int rate = sample_rate_spin_->value();

    live_engine_->set_pitch_semitones(live_voice_panel_->params().pitch_semitones);
    live_engine_->set_mute_silence(mute_silence_check_->isChecked());
    live_engine_->set_dry(dry_check_->isChecked());

    std::string err;
    if (!live_engine_->start(in_dev, out_dev, rate, 1024, err)) {
        QMessageBox::critical(this, QStringLiteral("OCS Studio"),
                              QString::fromStdString(err));
        return;
    }
    live_button_->setText(QStringLiteral("Stop"));
    live_status_label_->setText(QStringLiteral("Running"));
    live_poll_timer_->start();
}

void MainWindow::on_live_poll() {
    std::vector<float> samples;
    live_engine_->consume_output(samples);
    if (!samples.empty()) {
        live_waveform_->set_samples(samples, sample_rate_spin_->value());
    }
}

// ---------------------------------------------------------------------------
// Transcript tab
// ---------------------------------------------------------------------------
QWidget* MainWindow::build_transcript_tab() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    auto* hint = new QLabel(QStringLiteral(
        "Paste a transcript above and redact names / email / phones below."));
    layout->addWidget(hint);

    transcript_edit_ = new QPlainTextEdit;
    transcript_edit_->setPlaceholderText(QStringLiteral("Original transcript..."));
    layout->addWidget(transcript_edit_, 1);

    auto* redact_row = new QHBoxLayout;
    auto* redact_button = new QPushButton(QStringLiteral("Redact"));
    connect(redact_button, &QPushButton::clicked, this, &MainWindow::on_redact_transcript);
    redact_row->addWidget(redact_button);
    redact_row->addStretch(1);
    layout->addLayout(redact_row);

    transcript_output_ = new QPlainTextEdit;
    transcript_output_->setReadOnly(true);
    transcript_output_->setPlaceholderText(QStringLiteral("Redacted transcript..."));
    layout->addWidget(transcript_output_, 1);
    return page;
}

void MainWindow::on_redact_transcript() {
    redact::RedactOptions opts;
    const std::string input = transcript_edit_->toPlainText().toStdString();
    std::vector<std::string> names;
    const std::string red = redact::redact_text(input, opts, names);
    transcript_output_->setPlainText(QString::fromStdString(red));
    if (!names.empty()) {
        status_label_->setText(
            QStringLiteral("Redacted %1 name(s)").arg(static_cast<int>(names.size())));
    }
}

}  // namespace ocs::ui
