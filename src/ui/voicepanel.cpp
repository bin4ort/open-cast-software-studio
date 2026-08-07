// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#include "ui/voicepanel.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <cctype>
#include <string>

#include "dsp/phase_vocoder.h"

namespace ocs::ui {

namespace {

// Offline heuristic for translating a natural-language voice description into
// DSP settings. Used when no local LLM is available (the LLM path lives in
// ai/llm.cpp and is wired in during the AI pass).
dsp::ShiftParams heuristic_from_prompt(const QString& text) {
    dsp::ShiftParams p{2.0, 0.0, false};
    const std::string t = text.toLower().toStdString();
    auto has = [&](const char* w) { return t.find(w) != std::string::npos; };

    double base = 2.0, formant = 0.0;
    if (has("male") || has("man") || has("deep") || has("deepen") ||
        has("low") || has("bass") || has("masculine")) {
        base = -5.0;
        formant = -4.0;
    } else if (has("female") || has("woman") || has("girl") || has("feminine") ||
               has("high")) {
        base = 5.0;
        formant = 4.0;
    } else if (has("robot") || has("robotic") || has("synthetic") ||
               has("machine")) {
        base = 4.0;
        formant = 2.0;
    }
    if (has("young") || has("child") || has("kid") || has("small")) {
        base += 3.0;
        formant += 2.0;
    }
    if (has("old") || has("mature") || has("elder") || has("grand")) {
        base -= 2.0;
        formant -= 2.0;
    }
    if (has("chipmunk") || has("cartoon")) {
        base = 12.0;
        formant = 12.0;
    }
    p.pitch_semitones = base;
    p.formant_semitones = formant;
    return p;
}

}  // namespace

VoicePanel::VoicePanel(bool live, QWidget* parent)
    : QWidget(parent), live_(live) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* form = new QFormLayout;

    preset_combo_ = new QComboBox;
    preset_combo_->addItem(QStringLiteral("(custom)"));
    for (const auto& p : dsp::presets()) {
        preset_combo_->addItem(QString::fromStdString(p.name));
    }
    connect(preset_combo_, &QComboBox::currentIndexChanged, this,
            &VoicePanel::on_preset_changed);
    form->addRow(QStringLiteral("Preset:"), preset_combo_);

    gender_combo_ = new QComboBox;
    gender_combo_->addItem(QStringLiteral("Generic"));
    gender_combo_->addItem(QStringLiteral("Male"));
    gender_combo_->addItem(QStringLiteral("Female"));
    gender_combo_->addItem(QStringLiteral("Custom"));
    connect(gender_combo_, &QComboBox::currentIndexChanged, this,
            &VoicePanel::on_gender_changed);
    form->addRow(QStringLiteral("Target voice:"), gender_combo_);

    source_combo_ = new QComboBox;
    source_combo_->addItem(QStringLiteral("DSP (pitch/formant)"));
    source_combo_->addItem(QStringLiteral("AI (system prompt)"));
    connect(source_combo_, &QComboBox::currentIndexChanged, this,
            &VoicePanel::on_source_changed);
    form->addRow(QStringLiteral("Voice source:"), source_combo_);

    pitch_spin_ = new QDoubleSpinBox;
    pitch_spin_->setRange(-24.0, 24.0);
    pitch_spin_->setValue(2.0);
    pitch_spin_->setSuffix(QStringLiteral(" st"));
    connect(pitch_spin_, &QDoubleSpinBox::valueChanged, this,
            [this](double) { emit changed(); });
    form->addRow(QStringLiteral("Pitch:"), pitch_spin_);

    formant_spin_ = new QDoubleSpinBox;
    formant_spin_->setRange(-24.0, 24.0);
    formant_spin_->setValue(0.0);
    formant_spin_->setSuffix(QStringLiteral(" st"));
    connect(formant_spin_, &QDoubleSpinBox::valueChanged, this,
            [this](double) { emit changed(); });
    form->addRow(QStringLiteral("Formants:"), formant_spin_);
    if (live_) formant_spin_->setEnabled(false);

    dsp_panel_ = new QWidget(this);
    dsp_panel_->setLayout(form);

    // AI panel: system prompt + translate.
    ai_panel_ = new QWidget(this);
    auto* ai_layout = new QVBoxLayout(ai_panel_);
    ai_layout->setContentsMargins(0, 0, 0, 0);
    prompt_edit_ = new QPlainTextEdit;
    prompt_edit_->setFixedHeight(70);
    prompt_edit_->setPlaceholderText(QStringLiteral(
        "Describe the voice, e.g. \"young woman from London, slightly deep\""));
    translate_button_ = new QPushButton(QStringLiteral("Translate to voice"));
    connect(translate_button_, &QPushButton::clicked, this,
            &VoicePanel::on_translate);
    ai_status_ = new QLabel(QStringLiteral(
        "No local LLM detected - using built-in keyword matching."));
    ai_status_->setWordWrap(true);
    ai_layout->addWidget(prompt_edit_);
    ai_layout->addWidget(translate_button_);
    ai_layout->addWidget(ai_status_);
    ai_panel_->hide();

    layout->addWidget(dsp_panel_);
    layout->addWidget(ai_panel_);
}

dsp::ShiftParams VoicePanel::params() const {
    dsp::ShiftParams p;
    p.pitch_semitones = pitch_spin_->value();
    p.formant_semitones = formant_spin_->value();
    p.enable_denoise = false;
    return p;
}

dsp::Gender VoicePanel::gender() const {
    return static_cast<dsp::Gender>(gender_combo_->currentIndex());
}

bool VoicePanel::ai_mode() const {
    return source_combo_->currentIndex() == 1;
}

QString VoicePanel::prompt() const {
    return prompt_edit_->toPlainText();
}

void VoicePanel::apply_shift(const dsp::ShiftParams& p) {
    applying_profile_ = true;
    pitch_spin_->setValue(p.pitch_semitones);
    formant_spin_->setValue(p.formant_semitones);
    applying_profile_ = false;
    emit changed();
}

void VoicePanel::on_gender_changed(int index) {
    const auto g = static_cast<dsp::Gender>(index);
    if (g == dsp::Gender::Custom) return;
    applying_profile_ = true;
    const dsp::GenderProfile prof = dsp::profile_for(g);
    pitch_spin_->setValue(prof.pitch_semitones);
    formant_spin_->setValue(prof.formant_semitones);
    applying_profile_ = false;
    emit changed();
}

void VoicePanel::on_source_changed(int index) {
    const bool ai = index == 1;
    dsp_panel_->setVisible(!ai);
    ai_panel_->setVisible(ai);
    emit changed();
}

void VoicePanel::on_translate() {
    const dsp::ShiftParams p = heuristic_from_prompt(prompt_edit_->toPlainText());
    applying_profile_ = true;
    pitch_spin_->setValue(p.pitch_semitones);
    formant_spin_->setValue(p.formant_semitones);
    applying_profile_ = false;
    prompt_translated_ = true;
    ai_status_->setText(QStringLiteral("Translated: pitch %1 st, formants %2 st")
                            .arg(p.pitch_semitones)
                            .arg(p.formant_semitones));
    emit changed();
}

void VoicePanel::on_preset_changed(int index) {
    if (index <= 0) return;  // "(custom)"
    const auto& ps = dsp::presets();
    if (index - 1 < static_cast<int>(ps.size())) {
        apply_shift(ps[index - 1].params);
    }
}

}  // namespace ocs::ui
