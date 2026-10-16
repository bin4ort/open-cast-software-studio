// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#include "ui/mainwindow.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <chrono>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>

#include "ai/correction.h"
#include "ai/llm.h"
#include "anonymize/redactor.h"
#include "audio/device.h"
#include "audio/fileio.h"
#include "dsp/compare.h"
#include "dsp/pipeline.h"
#include "processing/job.h"
#include "transcribe/whisper.h"
#include "tts.h"
#include "ui/livepanel.h"
#include "ui/settingspanel.h"
#include "ui/voicepanel.h"
#include "ui/waveform.h"
#include "util/json.h"

namespace ocs::ui {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("OCS Studio"));
    resize(920, 640);

    auto* tabs = new QTabWidget(this);
    tabs->addTab(build_process_tab(), QStringLiteral("Process file"));
    tabs->addTab(build_live_tab(), QStringLiteral("Live masking"));
    tabs->addTab(build_transcript_tab(), QStringLiteral("Transcript"));
    tabs->addTab(build_settings_tab(), QStringLiteral("Settings"));
    setCentralWidget(tabs);

    live_engine_ = new LiveEngine;
    process_poll_timer_ = new QTimer(this);
    process_poll_timer_->setInterval(100);
    connect(process_poll_timer_, &QTimer::timeout, this, &MainWindow::on_process_poll);

    live_poll_timer_ = new QTimer(this);
    live_poll_timer_->setInterval(40);
    connect(live_poll_timer_, &QTimer::timeout, this, &MainWindow::on_live_poll);

    load_settings();
    apply_ai_settings();
}

MainWindow::~MainWindow() {
    // A finished std::thread stays joinable; joining here (even after the
    // work completed) prevents std::terminate on destruction.
    if (process_thread_.joinable()) process_thread_.join();
    // Wait for detached worker threads (preview/record/replay) so they never
    // touch members of a destroyed window.
    while (worker_threads_.load() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (live_engine_) {
        live_engine_->stop();
        delete live_engine_;
        live_engine_ = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Settings persistence
// ---------------------------------------------------------------------------
namespace {

QString settings_path() {
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) +
           QStringLiteral("/ocs_studio/settings.json");
}

}  // namespace

void MainWindow::save_settings() {
    std::map<std::string, util::Json> m;
    m["voice"] = voice_panel_->snapshot();
    m["live_voice"] = live_voice_panel_->snapshot();
    m["mode"] = util::Json(mode_combo_->currentIndex());
    m["denoise"] = util::Json(denoise_check_->isChecked());
    m["transcribe"] = util::Json(transcribe_check_->isChecked());
    m["in_device"] = util::Json(in_device_combo_->currentData().toInt());
    m["out_device"] = util::Json(out_device_combo_->currentData().toInt());
    m["output_mode"] = util::Json(output_mode_combo_->currentIndex());
    m["sample_rate"] = util::Json(sample_rate_spin_->value());
    m["mute_silence"] = util::Json(mute_silence_check_->isChecked());
    m["dry"] = util::Json(dry_check_->isChecked());
    m["ai_regen"] = util::Json(ai_regen_check_->isChecked());
    m["whisper_model"] = util::Json(whisper_model_edit_->text().toStdString());
    m["ai_settings"] = settings_panel_->snapshot();
    m["win_w"] = util::Json(width());
    m["win_h"] = util::Json(height());

    const util::Json root(std::move(m));
    const QString path = settings_path();
    QDir().mkpath(QFileInfo(path).absolutePath());
    std::ofstream f(path.toStdString(), std::ios::trunc);
    if (f) f << root.dump();
}

void MainWindow::load_settings() {
    try {
        const util::Json root = util::Json::parse_file(settings_path().toStdString());
        if (!root.is_object()) return;
        if (root.contains("voice")) voice_panel_->restore(root.at("voice"));
        if (root.contains("live_voice")) live_voice_panel_->restore(root.at("live_voice"));
        if (root.contains("mode")) mode_combo_->setCurrentIndex(
            static_cast<int>(root.at("mode").as_number()));
        if (root.contains("denoise")) denoise_check_->setChecked(root.at("denoise").as_bool());
        if (root.contains("transcribe")) transcribe_check_->setChecked(root.at("transcribe").as_bool());
        if (root.contains("in_device")) in_device_combo_->setCurrentIndex(
            in_device_combo_->findData(static_cast<int>(root.at("in_device").as_number())));
        if (root.contains("out_device")) out_device_combo_->setCurrentIndex(
            out_device_combo_->findData(static_cast<int>(root.at("out_device").as_number())));
        if (root.contains("output_mode")) output_mode_combo_->setCurrentIndex(
            static_cast<int>(root.at("output_mode").as_number()));
        if (root.contains("sample_rate")) sample_rate_spin_->setValue(
            static_cast<int>(root.at("sample_rate").as_number()));
        if (root.contains("mute_silence")) mute_silence_check_->setChecked(root.at("mute_silence").as_bool());
        if (root.contains("dry")) dry_check_->setChecked(root.at("dry").as_bool());
        if (root.contains("ai_regen")) ai_regen_check_->setChecked(root.at("ai_regen").as_bool());
        if (root.contains("whisper_model")) whisper_model_edit_->setText(
            QString::fromStdString(root.at("whisper_model").as_string()));
        if (root.contains("ai_settings")) settings_panel_->restore(root.at("ai_settings"));
        if (root.contains("win_w") && root.contains("win_h")) {
            resize(static_cast<int>(root.at("win_w").as_number()),
                   static_cast<int>(root.at("win_h").as_number()));
        }
    } catch (...) {
        // First run or corrupt settings: ignore.
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    save_settings();
    QMainWindow::closeEvent(event);
}

// ---------------------------------------------------------------------------
// Process tab
// ---------------------------------------------------------------------------
QWidget* MainWindow::build_process_tab() {
    // Everything lives in a scroll area so the controls stay reachable even
    // when the AI panel expands the form on small screens.
    auto* page = new QWidget;
    auto* page_layout = new QVBoxLayout(page);
    page_layout->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* inner = new QWidget;
    auto* layout = new QVBoxLayout(inner);

    // Section 1: Files
    auto* files_frame = new QFrame;
    files_frame->setFrameShape(QFrame::StyledPanel);
    files_frame->setFrameShadow(QFrame::Raised);
    auto* files_layout = new QVBoxLayout(files_frame);
    files_layout->setContentsMargins(0, 0, 0, 0);
    files_layout->setSpacing(10);

    auto* files_form = new QFormLayout;
    files_form->setSpacing(8);

    auto* pick_row = new QHBoxLayout;
    input_path_ = new QLineEdit;
    input_path_->setPlaceholderText(QStringLiteral("Select input audio"));
    auto* pick = new QPushButton(QStringLiteral("Browse"));
    pick->setMinimumWidth(80);
    connect(pick, &QPushButton::clicked, this, &MainWindow::on_pick_input);
    pick_row->addWidget(input_path_, 1);
    pick_row->addWidget(pick);
    files_form->addRow(QStringLiteral("Input"), pick_row);

    output_path_ = new QLineEdit;
    output_path_->setPlaceholderText(QStringLiteral("Output saved here"));
    files_form->addRow(QStringLiteral("Output"), output_path_);
    files_layout->addLayout(files_form);
    layout->addWidget(files_frame, 1);

    // Section 2: Voice
    auto* voice_frame = new QFrame;
    voice_frame->setFrameShape(QFrame::StyledPanel);
    voice_frame->setFrameShadow(QFrame::Raised);
    auto* voice_layout = new QVBoxLayout(voice_frame);
    voice_layout->setContentsMargins(0, 0, 0, 0);
    voice_panel_ = new VoicePanel(false);
    voice_layout->addWidget(voice_panel_);
    layout->addWidget(voice_frame, 1);

    // Section 3: Voice Preview
    auto* preview_frame = new QFrame;
    preview_frame->setFrameShape(QFrame::StyledPanel);
    preview_frame->setFrameShadow(QFrame::Raised);
    auto* preview_layout = new QVBoxLayout(preview_frame);
    preview_layout->setContentsMargins(0, 0, 0, 0);
    preview_layout->setSpacing(8);

    preview_phrase_combo_ = new QComboBox;
    preview_phrase_combo_->addItem(QStringLiteral("Use custom text"), "");
    preview_phrase_combo_->addItem(QStringLiteral("Hello, my name is Alex and I am here to help you."));
    preview_phrase_combo_->addItem(QStringLiteral("Welcome back to the show, everyone!"));
    preview_phrase_combo_->addItem(QStringLiteral("In a world of endless possibilities, one voice can change everything."));
    connect(preview_phrase_combo_, &QComboBox::currentIndexChanged, this,
            [this](int i) {
                if (i > 0) preview_text_edit_->setPlainText(preview_phrase_combo_->currentText());
            });
    preview_text_edit_ = new QPlainTextEdit;
    preview_text_edit_->setFixedHeight(60);
    preview_text_edit_->setPlaceholderText(QStringLiteral("Type to preview"));
    preview_button_ = new QPushButton(QStringLiteral("Preview"));
    connect(preview_button_, &QPushButton::clicked, this, &MainWindow::on_preview_text);
    preview_status_ = new QLabel(QStringLiteral("espeak-ng required"));
    preview_status_->setWordWrap(true);
    preview_status_->setMaximumWidth(200);

    auto* preview_top = new QHBoxLayout;
    preview_top->addWidget(preview_phrase_combo_);
    preview_layout->addLayout(preview_top);

    auto* preview_mid = new QHBoxLayout;
    preview_mid->addWidget(preview_text_edit_);
    preview_layout->addLayout(preview_mid);

    auto* preview_bot = new QHBoxLayout;
    preview_bot->addWidget(preview_button_);
    preview_bot->addWidget(preview_status_, 1);
    preview_layout->addLayout(preview_bot);
    layout->addWidget(preview_frame, 1);

    // Section 4: Options
    auto* opts_frame = new QFrame;
    opts_frame->setFrameShape(QFrame::StyledPanel);
    opts_frame->setFrameShadow(QFrame::Raised);
    auto* opts_layout = new QVBoxLayout(opts_frame);
    opts_layout->setContentsMargins(0, 0, 0, 0);
    opts_layout->setSpacing(10);

    auto* mode_row = new QHBoxLayout;
    mode_combo_ = new QComboBox;
    mode_combo_->addItem(QStringLiteral("Global"), "global");
    mode_combo_->addItem(QStringLiteral("Selective"), "selective");
    mode_combo_->addItem(QStringLiteral("Hybrid"), "hybrid");
    mode_row->addWidget(new QLabel(QStringLiteral("Mode: ")));
    mode_row->addStretch();
    mode_row->addWidget(mode_combo_);
    opts_layout->addLayout(mode_row);

    auto* opts_check_row = new QHBoxLayout;
    denoise_check_ = new QCheckBox(QStringLiteral("Spectral denoising"));
    transcribe_check_ = new QCheckBox(QStringLiteral("Transcribe + redact"));
    opts_check_row->addWidget(denoise_check_);
    opts_check_row->addWidget(transcribe_check_);
    opts_layout->addLayout(opts_check_row);
    layout->addWidget(opts_frame, 1);

    // Section 5: Process
    auto* process_frame = new QFrame;
    process_frame->setFrameShape(QFrame::StyledPanel);
    process_frame->setFrameShadow(QFrame::Raised);
    auto* process_layout = new QVBoxLayout(process_frame);
    process_layout->setContentsMargins(0, 0, 0, 0);
    process_layout->setSpacing(12);

    auto* process_top = new QHBoxLayout;
    process_button_ = new QPushButton(QStringLiteral("Process"));
    process_button_->setMinimumWidth(120);
    connect(process_button_, &QPushButton::clicked, this, &MainWindow::on_process);
    process_top->addWidget(process_button_);
    status_label_ = new QLabel(QStringLiteral("Idle"));
    status_label_->setWordWrap(true);
    status_label_->setMinimumHeight(24);
    process_top->addWidget(status_label_, 1);
    process_layout->addLayout(process_top);
    layout->addWidget(process_frame, 1);

    // Section 6: Waveform
    auto* wave_frame = new QFrame;
    wave_frame->setFrameShape(QFrame::StyledPanel);
    wave_frame->setFrameShadow(QFrame::Raised);
    wave_frame->setFixedHeight(140);
    auto* wave_layout = new QVBoxLayout(wave_frame);
    wave_layout->setContentsMargins(0, 0, 0, 0);
    wave_layout->setSpacing(8);

    // Top row: waveforms
    auto* wave_top = new QHBoxLayout;
    wave_top->setContentsMargins(0, 0, 0, 0);
    wave_top->setSpacing(8);
    input_waveform_ = new Waveform;
    output_waveform_ = new Waveform;
    wave_top->addWidget(input_waveform_, 1);
    wave_top->addWidget(output_waveform_, 1);
    wave_layout->addLayout(wave_top);

    // Bottom row: controls inside the waveform frame
    auto* wave_controls = new QHBoxLayout;
    wave_controls->setContentsMargins(8, 4, 8, 4);
    wave_controls->setSpacing(8);

    compare_button_ = new QPushButton(QStringLiteral("Compare"));
    compare_button_->setCheckable(true);
    compare_button_->setMinimumWidth(100);
    connect(compare_button_, &QPushButton::toggled, this,
            &MainWindow::on_compare_toggled);
    wave_controls->addWidget(compare_button_);

    correct_button_ = new QPushButton(QStringLiteral("AI Correct"));
    correct_button_->setEnabled(false);
    connect(correct_button_, &QPushButton::clicked, this,
            &MainWindow::on_correct_with_ai);
    correct_button_->setToolTip(QStringLiteral(
        "AI analyzes the difference and suggests voice corrections. "
        "Click to apply and re-process."));
    correct_button_->setStyleSheet(
        "QPushButton { "
        "    background-color: #3a7bd5; "
        "    color: white; "
        "    border: 1px solid #2a5a8a; "
        "    border-radius: 4px; "
        "    padding: 4px 12px; "
        "    font-size: 11px; "
        "} "
        "QPushButton:hover { "
        "    background-color: #4a8be5; "
        "    border: 1px solid #3a6a9a; "
        "} "
        "QPushButton:pressed { "
        "    background-color: #2a5a8a; "
        "    border: 1px solid #1a4a7a; "
        "}"
    );
    wave_controls->addWidget(correct_button_);

    wave_hint = new QLabel(QStringLiteral(
        "Grayscale = overlap. Use Compare to see differences."));
    wave_hint->setWordWrap(true);
    wave_hint->setMaximumWidth(280);
    wave_hint->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    wave_controls->addWidget(wave_hint, 1);
    wave_layout->addLayout(wave_controls);

    // AI Correction note - subtle info with custom styling
    auto* ai_note = new QLabel();
    ai_note->setWordWrap(true);
    ai_note->setAlignment(Qt::AlignLeft);
    ai_note->setStyleSheet(
        "QLabel { "
        "    color: #999; "
        "    font-size: 11px; "
        "    padding: 3px 4px; "
        "    line-height: 1.2; "
        "}"
    );
    ai_note->setText(
        "AI Correct analyzes original vs processed audio to suggest pitch and formant adjustments. "
        "It considers your target voice settings and explains why a correction was made.");
    layout->addWidget(ai_note);

    scroll->setWidget(inner);
    page_layout->addWidget(scroll);
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
        // A new input invalidates the previous comparison.
        output_waveform_->set_samples({}, 0);
        correction_note_.clear();
        correct_button_->setEnabled(false);
        compare_button_->setChecked(false);
        on_compare_toggled(false);
    }
}

void MainWindow::on_process() {
    if (processing_.load()) return;
    // Join a finished run so reassigning process_thread_ never hits
    // std::thread's joinable-assignment std::terminate.
    if (process_thread_.joinable()) process_thread_.join();

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
    correct_button_->setEnabled(false);

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
    correct_button_->setEnabled(!input_waveform_->samples().empty() &&
                                !output_waveform_->samples().empty());

    if (!process_ok_) {
        status_label_->setText(QStringLiteral("Error: ") +
                               QString::fromStdString(process_message_));
        QMessageBox::critical(this, QStringLiteral("OCS Studio"),
                              QString::fromStdString(process_message_));
        return;
    }
    status_label_->setText(QStringLiteral("Done"));
    if (!correction_note_.empty()) {
        status_label_->setText(QStringLiteral("Done - AI correction applied: ") +
                               QString::fromStdString(correction_note_));
        correction_note_.clear();
    }
    if (!process_message_.empty() && process_message_ != "ok") {
        status_label_->setText(QString::fromStdString(process_message_));
    }

    std::string err;
    AudioData ad = audio::read_file(output_path_->text().toStdString(), &err);
    if (!ad.samples.empty()) {
        output_waveform_->set_samples(ad.samples, ad.sample_rate);
    }
    if (compare_button_->isChecked()) on_compare_toggled(true);
    if (!process_segments_.empty()) {
        transcript_edit_->clear();
        for (const auto& seg : process_segments_) {
            transcript_edit_->appendPlainText(QString::fromStdString(seg.text));
        }
    }
}

// ---------------------------------------------------------------------------
// Waveform compare + AI correction
// ---------------------------------------------------------------------------
void MainWindow::on_compare_toggled(bool on) {
    if (on) {
        // Vivid main = the graph itself; grayscale overlay = the other
        // recording, so differences stand out.
        input_waveform_->set_overlay(output_waveform_->samples());
        output_waveform_->set_overlay(input_waveform_->samples());
    } else {
        input_waveform_->set_overlay({});
        output_waveform_->set_overlay({});
    }
    input_waveform_->set_compare_mode(on);
    output_waveform_->set_compare_mode(on);
}

void MainWindow::on_correct_with_ai() {
    if (correcting_.load() || processing_.load()) return;
    if (input_waveform_->samples().empty() || output_waveform_->samples().empty()) {
        status_label_->setText(QStringLiteral(
            "Load a file and process it first - the correction needs both "
            "the original and the processed waveform."));
        return;
    }
    if (input_path_->text().isEmpty()) {
        status_label_->setText(QStringLiteral("Pick an input file first."));
        return;
    }

    correcting_.store(true);
    correct_button_->setEnabled(false);
    status_label_->setText(QStringLiteral("Analyzing waveforms and asking the AI..."));

    const std::vector<float> orig = input_waveform_->samples();
    const std::vector<float> proc = output_waveform_->samples();
    const dsp::ShiftParams current = voice_panel_->params();
    const dsp::Gender target = voice_panel_->gender();

    spawn_worker([this, orig, proc, current, target] {
        const dsp::AudioDiff diff = dsp::analyze_diff(orig, proc, 128);
        dsp::ShiftParams corrected;
        std::string reason;
        const bool ok =
            ai::suggest_correction(diff, current, target, corrected, reason);
        QMetaObject::invokeMethod(this, [this, ok, corrected, reason] {
            correcting_.store(false);
            correct_button_->setEnabled(true);
            if (!ok) {
                status_label_->setText(QStringLiteral(
                    "No AI backend responded - no correction (check the "
                    "Settings tab)."));
                return;
            }
            // Apply the suggested voice configuration.
            voice_panel_->apply_shift(corrected);
            denoise_check_->setChecked(corrected.enable_denoise);
            correction_note_ = QStringLiteral("pitch %1 st, formants %2 st, "
                                              "denoise %3%4")
                                   .arg(corrected.pitch_semitones)
                                   .arg(corrected.formant_semitones)
                                   .arg(corrected.enable_denoise ? "on" : "off")
                                   .arg(reason.empty()
                                            ? QString()
                                            : QStringLiteral(" (%1)")
                                                  .arg(QString::fromStdString(reason)))
                                   .toStdString();
            status_label_->setText(QStringLiteral(
                "AI correction applied - re-processing with the new voice..."));
            // Re-run the pipeline with the corrected settings; the updated
            // waveforms appear when the process finishes.
            on_process();
        }, Qt::QueuedConnection);
    });
}
// Runs `fn` on a detached worker thread. The destructor waits for all such
// threads to finish so detached work never touches a destroyed window.
void MainWindow::spawn_worker(std::function<void()> fn) {
    ++worker_threads_;
    std::thread([this, fn = std::move(fn)] {
        fn();
        --worker_threads_;
    }).detach();
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
    spawn_worker([this, out, out_dev] {
        std::string perr;
        audio::play_buffer(out, out_dev, &perr);
        QMetaObject::invokeMethod(this, [this, perr] {
            preview_button_->setEnabled(true);
            preview_status_->setText(
                perr.empty() ? QStringLiteral("Preview done.") : QString::fromStdString(perr));
        }, Qt::QueuedConnection);
    });
}

// ---------------------------------------------------------------------------
// Mic test
// ---------------------------------------------------------------------------
void MainWindow::on_record_raw() { record_test(false); }
void MainWindow::on_record_anon() { record_test(true); }

void MainWindow::record_test(bool anonymized) {
    if (recording_.load()) return;
    if (live_engine_->running()) {
        QMessageBox::warning(this, QStringLiteral("OCS Studio"),
                             QStringLiteral("Stop live masking before a mic test."));
        return;
    }
    const int in_dev = in_device_combo_->currentData().toInt();
    const int rate = sample_rate_spin_->value();
    if (in_dev < 0) {
        mic_test_status_->setText(QStringLiteral("No input device selected."));
        return;
    }
    recording_.store(true);
    record_raw_button_->setEnabled(false);
    record_anon_button_->setEnabled(false);
    const int seconds = test_seconds_spin_->value();
    mic_test_status_->setText(
        QStringLiteral("Recording %1s...").arg(seconds));

    spawn_worker([this, in_dev, rate, anonymized, seconds] {
        std::string err;
        const AudioData rec =
            audio::record_seconds(seconds, in_dev, rate, &err);
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
    });
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
    spawn_worker([this, out_dev] {
        std::string err;
        audio::play_buffer(mic_test_last_, out_dev, &err);
        QMetaObject::invokeMethod(this, [this, err] {
            replay_button_->setEnabled(true);
            mic_test_status_->setText(err.empty() ? QStringLiteral("Replay done.")
                                                  : QString::fromStdString(err));
        }, Qt::QueuedConnection);
    });
}

// ---------------------------------------------------------------------------
// Live tab
// ---------------------------------------------------------------------------
QWidget* MainWindow::build_live_tab() {
    auto* page = new QWidget;
    auto* page_layout = new QVBoxLayout(page);
    page_layout->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* inner = new QWidget;
    auto* layout = new QVBoxLayout(inner);

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

    auto* regen_group = new QGroupBox(QStringLiteral("AI regeneration"));
    auto* regen_form = new QFormLayout(regen_group);
    ai_regen_check_ = new QCheckBox(
        QStringLiteral("Regenerate with AI (transcribe + re-synthesize)"));
    regen_form->addRow(QStringLiteral(""), ai_regen_check_);
    whisper_model_edit_ = new QLineEdit;
    whisper_model_edit_->setPlaceholderText(QStringLiteral("Path to a whisper .bin model"));
    whisper_model_edit_->setText(QString::fromStdString(transcribe::find_whisper_model()));
    regen_form->addRow(QStringLiteral("Whisper model:"), whisper_model_edit_);
    layout->addWidget(regen_group);

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

    scroll->setWidget(inner);
    page_layout->addWidget(scroll);
    return page;
}

// ---------------------------------------------------------------------------
// Settings tab
// ---------------------------------------------------------------------------
QWidget* MainWindow::build_settings_tab() {
    settings_panel_ = new SettingsPanel;
    connect(settings_panel_, &SettingsPanel::settingsChanged, this, [this] {
        apply_ai_settings();
        save_settings();
    });
    return settings_panel_;
}

void MainWindow::apply_ai_settings() {
    if (!settings_panel_) return;
    llm::set_config(settings_panel_->config());
    // The voice panels show connectivity status; refresh them in the
    // background so the new addresses take effect immediately.
    if (voice_panel_) voice_panel_->refresh_ai_status();
    if (live_voice_panel_) live_voice_panel_->refresh_ai_status();
}

void MainWindow::on_live_start_stop() {
    if (live_engine_->running()) {
        live_engine_->stop();
        live_button_->setText(QStringLiteral("Start"));
        live_status_label_->setText(QStringLiteral("Stopped"));
        live_poll_timer_->stop();
        record_raw_button_->setEnabled(true);
        record_anon_button_->setEnabled(true);
        return;
    }
    if (recording_.load()) {
        QMessageBox::warning(this, QStringLiteral("OCS Studio"),
                             QStringLiteral("Finish the mic test first."));
        return;
    }

    const int in_dev = in_device_combo_->currentData().toInt();
    const int out_dev = out_device_combo_->currentData().toInt();
    const int rate = sample_rate_spin_->value();

    live_engine_->set_pitch_semitones(live_voice_panel_->params().pitch_semitones);
    live_engine_->set_mute_silence(mute_silence_check_->isChecked());
    live_engine_->set_dry(dry_check_->isChecked());
    live_engine_->set_ai_regen(ai_regen_check_->isChecked());
    live_engine_->set_whisper_model(whisper_model_edit_->text().toStdString());
    live_engine_->set_regen_voice(live_voice_panel_->gender(),
                                  live_voice_panel_->params());

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
    record_raw_button_->setEnabled(false);
    record_anon_button_->setEnabled(false);
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
