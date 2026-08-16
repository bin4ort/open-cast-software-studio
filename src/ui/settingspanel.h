// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#pragma once

#include <QWidget>

#include <atomic>

#include "ai/llm.h"
#include "util/json.h"

class QCheckBox;
class QLineEdit;
class QLabel;
class QPushButton;

namespace ocs::ui {

// Settings tab: AI backend connectivity (Ollama / llama.cpp addresses, e.g.
// for a remote server or a second computer) plus manual and automatic launch
// of the backend process from a configurable working directory.
class SettingsPanel : public QWidget {
    Q_OBJECT

public:
    explicit SettingsPanel(QWidget* parent = nullptr);
    ~SettingsPanel() override;

    // Current settings as an LLM config (addresses + launch options).
    ocs::llm::Config config() const;

    // Fills the fields from a config (used when loading persisted settings).
    void set_config(const ocs::llm::Config& cfg);

    // Persistence: current settings <-> JSON object.
    util::Json snapshot() const;
    void restore(const util::Json& obj);

    // Asynchronous: probes both endpoints and shows the result.
    void test_connection();

signals:
    // Emitted whenever any setting changed (MainWindow applies + saves).
    void settingsChanged();

private slots:
    void on_test_connection();
    void on_launch_now();
    void on_browse_dir();

private:
    template <typename Fn>
    void run_worker(Fn fn);

    QLineEdit* ollama_edit_ = nullptr;
    QLineEdit* llama_edit_ = nullptr;
    QLabel* test_status_ = nullptr;
    QPushButton* test_button_ = nullptr;
    QCheckBox* auto_launch_check_ = nullptr;
    QLineEdit* launch_cmd_edit_ = nullptr;
    QLineEdit* launch_dir_edit_ = nullptr;
    QPushButton* launch_button_ = nullptr;
    QLabel* launch_status_ = nullptr;
    std::atomic<bool> testing_{false};
    std::atomic<int> worker_threads_{0};
};

}  // namespace ocs::ui
