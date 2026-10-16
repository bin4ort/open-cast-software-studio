// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#pragma once

#include <QMainWindow>

#include <atomic>
#include <functional>
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
class SettingsPanel;

// Main application window: File processing, Live masking, Transcript and
// Settings tabs.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void on_pick_input();
    void on_process();
    void on_process_poll();
    void on_live_start_stop();
    void on_live_poll();
    void on_redact_transcript();
    void on_preview_text();
    void on_record_raw();
    void on_record_anon();
    void on_replay();
    void record_test(bool anonymized);
    void on_compare_toggled(bool on);
    void on_correct_with_ai();

private:
    QWidget* build_process_tab();
    QWidget* build_live_tab();
    QWidget* build_transcript_tab();
    QWidget* build_settings_tab();
    void save_settings();
    void load_settings();
    // Applies the settings-tab values to the LLM connectivity config and
    // refreshes any UI that depends on it.
    void apply_ai_settings();
    void spawn_worker(std::function<void()> fn);

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
    QPushButton* compare_button_ = nullptr;
    QPushButton* correct_button_ = nullptr;
    QLabel* wave_hint = nullptr;
    QTimer* process_poll_timer_ = nullptr;

    // AI correction of the processed output (fed with the waveform diff).
    std::atomic<bool> correcting_{false};
    std::string correction_note_;

    // Voice preview (P2).
    QPlainTextEdit* preview_text_edit_ = nullptr;
    QComboBox* preview_phrase_combo_ = nullptr;
    QPushButton* preview_button_ = nullptr;
    QLabel* preview_status_ = nullptr;

    std::thread process_thread_;
    std::atomic<bool> processing_{false};
    std::atomic<bool> process_done_{false};
    std::atomic<int> worker_threads_{0};
    bool process_ok_ = false;
    std::string process_message_;
    std::vector<Segment> process_segments_;

    // Live tab.
    QComboBox* in_device_combo_ = nullptr;
    QComboBox* out_device_combo_ = nullptr;
    QComboBox* output_mode_combo_ = nullptr;
    VoicePanel* live_voice_panel_ = nullptr;
    QSpinBox* sample_rate_spin_ = nullptr;
    QCheckBox* mute_silence_check_ = nullptr;
    QCheckBox* dry_check_ = nullptr;
    QCheckBox* ai_regen_check_ = nullptr;
    QLineEdit* whisper_model_edit_ = nullptr;
    QPushButton* live_button_ = nullptr;
    QLabel* live_status_label_ = nullptr;
    Waveform* live_waveform_ = nullptr;
    QTimer* live_poll_timer_ = nullptr;
    LiveEngine* live_engine_ = nullptr;

    // Mic test (P3).
    QSpinBox* test_seconds_spin_ = nullptr;
    QPushButton* record_raw_button_ = nullptr;
    QPushButton* record_anon_button_ = nullptr;
    QPushButton* replay_button_ = nullptr;
    QLabel* mic_test_status_ = nullptr;
    AudioData mic_test_raw_;
    AudioData mic_test_anon_;
    AudioData mic_test_last_;
    std::atomic<bool> recording_{false};

    // Transcript tab.
    QPlainTextEdit* transcript_edit_ = nullptr;
    QPlainTextEdit* transcript_output_ = nullptr;

    // Settings tab.
    SettingsPanel* settings_panel_ = nullptr;
};

}  // namespace ocs::ui
