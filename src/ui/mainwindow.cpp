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
#include "dsp/pipeline.h"
#include "processing/job.h"
#include "tts.h"
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

    // Voice preview: hear the configured voice reading an example text.
    auto* preview_group = new QGroupBox(QStringLiteral("Voice preview"));
    auto* preview_layout = new QVBoxLayout(preview_group);
    preview_phrase_combo_ = new QComboBox;
    preview_phrase_combo_->addItem(QStringLiteral("Use custom text below"), "");
    preview_phrase_combo_->addItem(QStringLiteral("Hello, my name is Alex and I am here to help you."));
    preview_phrase_combo_->addItem(QStringLiteral("Welcome back to the show, everyone!"));
    preview_phrase_combo_->addItem(QStringLiteral("In a world of endless possibilities, one voice can change everything."));
    connect(preview_phrase_combo_, &QComboBox::currentIndexChanged, this,
            [this](int i) {
                if (i > 0) preview_text_edit_->setPlainText(preview_phrase_combo_->currentText());
            });
    preview_text_edit_ = new QPlainTextEdit;
    preview_text_edit_->setFixedHeight(70);
    preview_text_edit_->setPlaceholderText(QStringLiteral("Example text for the preview..."));
    preview_button_ = new QPushButton(QStringLiteral("Preview voice"));
    connect(preview_button_, &QPushButton::clicked, this, &MainWindow::on_preview_text);
    preview_status_ = new QLabel(QStringLiteral("espeak-ng required for preview."));
    preview_status_->setWordWrap(true);
    preview_layout->addWidget(preview_phrase_combo_);
    preview_layout->addWidget(preview_text_edit_);
    auto* preview_row = new QHBoxLayout;
    preview_row->addWidget(preview_button_);
    preview_row->addWidget(preview_status_, 1);
    preview_layout->addLayout(preview_row);
    layout->addWidget(preview_group);

    auto* opts_group = new QGroupBox(QStringLiteral("Options"));
    auto* opts_form = new QFormLayout(opts_group);

    mode_combo_ = new QComboBox;
    mode_combo_->addItem(QStringLiteral("Global (shift all speech)"), "global");
    mode_combo_->addItem(QStringLiteral("Selective (PII segments only)"), "selective");
    mode_combo_->addItem(QStringLiteral("Hybrid (AI gender + re-voiced phrases)"), "hybrid");
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
    const QString mode = mode_combo_->currentData().toString();
    opts.mode = dsp::AnonMode::Global;
    if (mode == QStringLiteral("selective")) opts.mode = dsp::AnonMode::Selective;
    else if (mode == QStringLiteral("hybrid")) opts.mode = dsp::AnonMode::Hybrid;
    opts.transcribe = transcribe_check_->isChecked() ||
                      opts.mode == dsp::AnonMode::Hybrid;
    opts.write_transcript = transcribe_check_->isChecked() ||
                            opts.mode == dsp::AnonMode::Hybrid;

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
// Voice preview
// ---------------------------------------------------------------------------
void MainWindow::on_preview_text() {
    QString text = preview_text_edit_->toPlainText().trimmed();
    if (text.isEmpty()) {
        preview_status_->setText(QStringLiteral("Enter some example text first."));
        return;
    }
    if (!tts::available()) {
        preview_status_->setText(QStringLiteral(
            "espeak-ng is not installed. Install it to enable voice preview."));
        return;
    }

    AudioData spoken;
    std::string err;
    if (!tts::synthesize(text.toStdString(), voice_panel_->gender(), spoken, err)) {
        preview_status_->setText(QString::fromStdString(err));
        return;
    }

    // Apply the configured voice shift so the preview matches the target.
    std::vector<double> in_d(spoken.samples.begin(), spoken.samples.end());
    const std::vector<double> shifted =
        dsp::shift_audio(in_d, spoken.sample_rate, voice_panel_->params());
    AudioData out;
    out.sample_rate = spoken.sample_rate;
    out.samples.reserve(shifted.size());
    for (double v : shifted) out.samples.push_back(static_cast<Sample>(v));

    const int out_dev = audio::default_output_device();
    if (out_dev < 0) {
        preview_status_->setText(QStringLiteral("No output device available."));
        return;
    }
    preview_status_->setText(QStringLiteral("Playing preview..."));
    preview_button_->setEnabled(false);
    std::thread([this, out, out_dev] {
        std::string perr;
        audio::play_buffer(out, out_dev, &perr);
        QMetaObject::invokeMethod(this, [this, perr] {
            preview_button_->setEnabled(true);
            preview_status_->setText(
                perr.empty() ? QStringLiteral("Preview done.") : QString::fromStdString(perr));
        }, Qt::QueuedConnection);
    }).detach();
}

// ---------------------------------------------------------------------------
// Mic test
// ---------------------------------------------------------------------------
void MainWindow::on_record_raw() { record_test(false); }
void MainWindow::on_record_anon() { record_test(true); }

void MainWindow::record_test(bool anonymized) {
    if (recording_.load()) return;
    const int in_dev = in_device_combo_->currentData().toInt();
    const int rate = sample_rate_spin_->value();
    if (in_dev < 0) {
        mic_test_status_->setText(QStringLiteral("No input device selected."));
        return;
    }
    recording_.store(true);
    record_raw_button_->setEnabled(false);
    record_anon_button_->setEnabled(false);
    mic_test_status_->setText(
        QStringLiteral("Recording %1s...").arg(test_seconds_spin_->value()));

    std::thread([this, in_dev, rate, anonymized] {
        std::string err;
        const AudioData rec =
            audio::record_seconds(test_seconds_spin_->value(), in_dev, rate, &err);
        if (anonymized && !rec.samples.empty()) {
            std::vector<double> in_d(rec.samples.begin(), rec.samples.end());
            dsp::ShiftParams p = live_voice_panel_->params();
            p.formant_semitones = 0.0;  // live path is pitch-only
            const std::vector<double> shifted =
                dsp::shift_audio(in_d, rec.sample_rate, p);
            mic_test_anon_.sample_rate = rec.sample_rate;
            mic_test_anon_.samples.clear();
            mic_test_anon_.samples.reserve(shifted.size());
            for (double v : shifted) {
                mic_test_anon_.samples.push_back(static_cast<Sample>(v));
            }
            mic_test_last_ = mic_test_anon_;
        } else {
            mic_test_raw_ = rec;
            mic_test_last_ = rec;
        }
        QString msg;
        if (rec.samples.empty()) {
            msg = QString::fromStdString(err.empty() ? "Recording failed" : err);
        } else {
            msg = anonymized ? QStringLiteral("Recorded anonymized test.")
                             : QStringLiteral("Recorded raw test.");
        }
        QMetaObject::invokeMethod(this, [this, msg] {
            recording_.store(false);
            record_raw_button_->setEnabled(true);
            record_anon_button_->setEnabled(true);
            replay_button_->setEnabled(!mic_test_last_.samples.empty());
            mic_test_status_->setText(msg);
        }, Qt::QueuedConnection);
    }).detach();
}

void MainWindow::on_replay() {
    if (mic_test_last_.samples.empty()) return;
    const int out_dev = audio::default_output_device();
    if (out_dev < 0) {
        mic_test_status_->setText(QStringLiteral("No output device."));
        return;
    }
    replay_button_->setEnabled(false);
    mic_test_status_->setText(QStringLiteral("Replaying..."));
    std::thread([this, out_dev] {
        std::string err;
        audio::play_buffer(mic_test_last_, out_dev, &err);
        QMetaObject::invokeMethod(this, [this, err] {
            replay_button_->setEnabled(true);
            mic_test_status_->setText(err.empty() ? QStringLiteral("Replay done.")
                                                  : QString::fromStdString(err));
        }, Qt::QueuedConnection);
    }).detach();
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

    output_mode_combo_ = new QComboBox;
    output_mode_combo_->addItem(QStringLiteral("Speaker"), 0);
    output_mode_combo_->addItem(QStringLiteral("Virtual mic (for apps)"), 1);
    output_mode_combo_->addItem(QStringLiteral("Speaker + virtual mic"), 2);
    dev_form->addRow(QStringLiteral("Send to:"), output_mode_combo_);
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

    auto* test_group = new QGroupBox(QStringLiteral("Mic test"));
    auto* test_form = new QFormLayout(test_group);
    test_seconds_spin_ = new QSpinBox;
    test_seconds_spin_->setRange(1, 10);
    test_seconds_spin_->setValue(3);
    test_seconds_spin_->setSuffix(QStringLiteral(" s"));
    test_form->addRow(QStringLiteral("Duration:"), test_seconds_spin_);

    auto* test_row = new QHBoxLayout;
    record_raw_button_ = new QPushButton(QStringLiteral("Record raw"));
    connect(record_raw_button_, &QPushButton::clicked, this, &MainWindow::on_record_raw);
    record_anon_button_ = new QPushButton(QStringLiteral("Record anonymized"));
    connect(record_anon_button_, &QPushButton::clicked, this, &MainWindow::on_record_anon);
    replay_button_ = new QPushButton(QStringLiteral("Replay last"));
    connect(replay_button_, &QPushButton::clicked, this, &MainWindow::on_replay);
    replay_button_->setEnabled(false);
    test_row->addWidget(record_raw_button_);
    test_row->addWidget(record_anon_button_);
    test_row->addWidget(replay_button_);
    test_form->addRow(QStringLiteral(""), test_row);

    mic_test_status_ = new QLabel(QStringLiteral("Record a test to check the mic."));
    mic_test_status_->setWordWrap(true);
    test_form->addRow(QStringLiteral(""), mic_test_status_);
    layout->addWidget(test_group);

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

    OutputMode mode = OutputMode::Speaker;
    const int mode_index = output_mode_combo_->currentData().toInt();
    if (mode_index == 1) mode = OutputMode::VirtualMic;
    else if (mode_index == 2) mode = OutputMode::Both;

    std::string err;
    if (!live_engine_->start(in_dev, out_dev, rate, 1024, mode, err)) {
        QMessageBox::critical(this, QStringLiteral("OCS Studio"),
                              QString::fromStdString(err));
        return;
    }
    live_button_->setText(QStringLiteral("Stop"));
    if (mode == OutputMode::Speaker) {
        live_status_label_->setText(QStringLiteral("Running (speaker)"));
    } else {
        live_status_label_->setText(QStringLiteral("Running (virtual mic active)"));
    }
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
