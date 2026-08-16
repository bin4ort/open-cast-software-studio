// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#pragma once

#include <QWidget>

#include <atomic>
#include <string>

#include "dsp/gender.h"
#include "dsp/pipeline.h"
#include "util/json.h"

class QComboBox;
class QDoubleSpinBox;
class QPlainTextEdit;
class QPushButton;
class QLabel;

namespace ocs::ui {

// Reusable voice settings: target gender, DSP vs AI source, pitch/formant
// sliders and an AI system-prompt box (with translate). Used by both the
// Process-file and Live-masking tabs.
class VoicePanel : public QWidget {
    Q_OBJECT

public:
    // `live` hides the formant control (the real-time shifter is pitch-only).
    explicit VoicePanel(bool live = false, QWidget* parent = nullptr);
    ~VoicePanel() override;

    dsp::ShiftParams params() const;
    dsp::Gender gender() const;
    bool ai_mode() const;
    QString prompt() const;
    bool prompt_translated() const { return prompt_translated_; }

    // Applies a shift preset (keeps the selected gender).
    void apply_shift(const dsp::ShiftParams& p);

    // Persistence: current settings <-> JSON object.
    util::Json snapshot() const;
    void restore(const util::Json& obj);

    // Refreshes the AI connectivity status label in the background. Safe to
    // call at any time (e.g. after the AI settings changed).
    void refresh_ai_status();

signals:
    void changed();

private slots:
    void on_gender_changed(int index);
    void on_source_changed(int index);
    void on_translate();
    void on_preset_changed(int index);
    void on_save_preset();
    void on_load_preset();

private:
    // Runs `fn` on a detached worker thread (used for AI calls that can take
    // a long time). The destructor waits for in-flight workers.
    template <typename Fn>
    void run_worker(Fn fn);

    bool live_;
    QComboBox* preset_combo_ = nullptr;
    QComboBox* gender_combo_ = nullptr;
    QComboBox* source_combo_ = nullptr;
    QDoubleSpinBox* pitch_spin_ = nullptr;
    QDoubleSpinBox* formant_spin_ = nullptr;
    QWidget* ai_panel_ = nullptr;
    QPlainTextEdit* prompt_edit_ = nullptr;
    QPushButton* translate_button_ = nullptr;
    QLabel* ai_status_ = nullptr;
    bool applying_profile_ = false;
    bool prompt_translated_ = false;
    std::atomic<bool> translating_{false};
    std::atomic<int> worker_threads_{0};
};

}  // namespace ocs::ui
