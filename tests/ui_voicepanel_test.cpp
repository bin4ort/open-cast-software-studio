// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
// GUI widget tests (run with QT_QPA_PLATFORM=offscreen).
#include <QApplication>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QWidget>

#include <cstdio>

#include "ui/settingspanel.h"
#include "ui/voicepanel.h"
#include "ui/waveform.h"
#include "util/json.h"

namespace {

int failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);    \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

// Finds the "Voice source" combo of a VoicePanel (the one with exactly the
// DSP/AI items) and the AI panel widget (parent of the prompt editor).
QComboBox* source_combo(ocs::ui::VoicePanel& vp) {
    for (QComboBox* cb : vp.findChildren<QComboBox*>()) {
        if (cb->count() == 2 && cb->itemText(1).contains("AI")) return cb;
    }
    return nullptr;
}

QWidget* ai_panel(ocs::ui::VoicePanel& vp) {
    QPlainTextEdit* prompt = vp.findChild<QPlainTextEdit*>();
    return prompt ? prompt->parentWidget() : nullptr;
}

void test_source_switch() {
    std::printf("source switch (DSP <-> AI)\n");
    ocs::ui::VoicePanel vp(false);
    vp.resize(400, 300);
    vp.show();
    QCoreApplication::processEvents();

    QComboBox* source = source_combo(vp);
    QWidget* panel = ai_panel(vp);
    CHECK(source != nullptr);
    CHECK(panel != nullptr);
    if (!source || !panel) return;

    source->setCurrentIndex(0);
    QCoreApplication::processEvents();
    CHECK(!panel->isVisible());

    // Switch to AI: the prompt panel appears.
    source->setCurrentIndex(1);
    QCoreApplication::processEvents();
    CHECK(panel->isVisible());

    // Switch back to DSP: the prompt panel must disappear again.
    source->setCurrentIndex(0);
    QCoreApplication::processEvents();
    CHECK(!panel->isVisible());
}

void test_snapshot_restore_round_trip() {
    std::printf("voice panel snapshot/restore\n");
    ocs::ui::VoicePanel vp(false);
    vp.resize(400, 300);
    vp.show();
    QCoreApplication::processEvents();

    QComboBox* source = source_combo(vp);
    QWidget* panel = ai_panel(vp);
    CHECK(source != nullptr);
    CHECK(panel != nullptr);
    if (!source || !panel) return;

    source->setCurrentIndex(1);
    QCoreApplication::processEvents();
    const ocs::util::Json snap = vp.snapshot();

    ocs::ui::VoicePanel vp2(false);
    vp2.resize(400, 300);
    vp2.show();
    vp2.restore(snap);
    QCoreApplication::processEvents();
    QComboBox* source2 = source_combo(vp2);
    CHECK(source2 != nullptr);
    if (source2) {
        CHECK(source2->currentIndex() == 1);
        CHECK(ai_panel(vp2)->isVisible());
    }
}

void test_settings_panel_round_trip() {
    std::printf("settings panel snapshot/config\n");
    ocs::ui::SettingsPanel sp;
    sp.resize(500, 300);
    sp.show();
    QCoreApplication::processEvents();

    ocs::llm::Config c;
    c.ollama_url = "http://192.168.1.50:11434";
    c.llama_url = "http://192.168.1.50:8080/";
    c.launch_command = "ollama serve";
    c.launch_dir = "/home/user/ai";
    c.auto_launch = true;
    sp.set_config(c);

    const ocs::util::Json snap = sp.snapshot();

    ocs::ui::SettingsPanel sp2;
    sp2.restore(snap);
    const ocs::llm::Config c2 = sp2.config();
    CHECK(c2.ollama_url == "http://192.168.1.50:11434");
    CHECK(c2.llama_url == "http://192.168.1.50:8080/");
    CHECK(c2.launch_command == "ollama serve");
    CHECK(c2.launch_dir == "/home/user/ai");
    CHECK(c2.auto_launch == true);
}

void test_waveform_compare() {
    std::printf("waveform compare overlay\n");
    ocs::ui::Waveform wf;
    wf.resize(320, 140);
    wf.show();
    std::vector<float> main(4800, 0.0f);
    std::vector<float> overlay(4800, 0.0f);
    for (int i = 0; i < 4800; ++i) {
        main[i] = 0.5f * std::sin(i * 0.1f);
        overlay[i] = 0.4f * std::sin(i * 0.1f + 0.3f);
    }
    wf.set_samples(main, 48000);
    CHECK(!wf.compare_mode());
    wf.set_overlay(overlay);
    wf.set_compare_mode(true);
    QCoreApplication::processEvents();
    CHECK(wf.compare_mode());
    CHECK(wf.samples().size() == main.size());
    CHECK(wf.overlay().size() == overlay.size());

    // Toggling compare off must not clear the buffers (the MainWindow does
    // that explicitly) and must render without issues.
    wf.set_compare_mode(false);
    QCoreApplication::processEvents();
    CHECK(!wf.compare_mode());
    CHECK(wf.overlay().size() == overlay.size());
}

}  // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    std::printf("OCS Studio GUI tests\n");
    test_source_switch();
    test_snapshot_restore_round_trip();
    test_settings_panel_round_trip();
    test_waveform_compare();
    if (failures == 0) {
        std::printf("GUI tests passed\n");
        return 0;
    }
    std::printf("%d GUI test failure(s)\n", failures);
    return 1;
}
