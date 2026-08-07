// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#pragma once

#include <QMainWindow>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include "common.h"
#include "dsp/pipeline.h"

class QTabWidget;
class QComboBox;
class QDoubleSpinBox;
class QCheckBox;
class QLineEdit;
class QPushButton;
class QPlainTextEdit;
class QLabel;
class QSpinBox;
class QTimer;

namespace ocs::ui {

class Waveform;
class LiveEngine;
class VoicePanel;

// Main application window: File processing, Live masking and Transcript tabs.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void on_pick_input();
    void on_process();
    void on_process_poll();
    void on_live_start_stop();
    void on_live_poll();
    void on_redact_transcript();

private:
    QWidget* build_process_tab();
    QWidget* build_live_tab();
    QWidget* build_transcript_tab();

    // Process tab.
    QLineEdit* input_path_ = nullptr;
    QLineEdit* output_path_ = nullptr;
    VoicePanel* voice_panel_ = nullptr;
    QComboBox* mode_combo_ = nullptr;
    QCheckBox* denoise_check_ = nullptr;
    QCheckBox* transcribe_check_ = nullptr;
    QPushButton* process_button_ = nullptr;
    QLabel* status_label_ = nullptr;
    Waveform* input_waveform_ = nullptr;
    Waveform* output_waveform_ = nullptr;
    QTimer* process_poll_timer_ = nullptr;

    std::thread process_thread_;
    std::atomic<bool> processing_{false};
    std::atomic<bool> process_done_{false};
    bool process_ok_ = false;
    std::string process_message_;
    std::vector<Segment> process_segments_;

    // Live tab.
    QComboBox* in_device_combo_ = nullptr;
    QComboBox* out_device_combo_ = nullptr;
    VoicePanel* live_voice_panel_ = nullptr;
    QSpinBox* sample_rate_spin_ = nullptr;
    QCheckBox* mute_silence_check_ = nullptr;
    QCheckBox* dry_check_ = nullptr;
    QPushButton* live_button_ = nullptr;
    QLabel* live_status_label_ = nullptr;
    Waveform* live_waveform_ = nullptr;
    QTimer* live_poll_timer_ = nullptr;
    LiveEngine* live_engine_ = nullptr;

    // Transcript tab.
    QPlainTextEdit* transcript_edit_ = nullptr;
    QPlainTextEdit* transcript_output_ = nullptr;
};

}  // namespace ocs::ui
