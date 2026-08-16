// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#include "ui/settingspanel.h"

#include <QCheckBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <chrono>
#include <map>
#include <thread>

namespace ocs::ui {

SettingsPanel::SettingsPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);

    auto* ai_group = new QGroupBox(QStringLiteral("AI backend connection"));
    auto* ai_form = new QFormLayout(ai_group);

    ollama_edit_ = new QLineEdit(QStringLiteral("http://localhost:11434"));
    ollama_edit_->setPlaceholderText(QStringLiteral("http://host:port"));
    connect(ollama_edit_, &QLineEdit::textChanged, this,
            [this](const QString&) { emit settingsChanged(); });
    ai_form->addRow(QStringLiteral("Ollama server:"), ollama_edit_);

    llama_edit_ = new QLineEdit(QStringLiteral("http://localhost:8080"));
    llama_edit_->setPlaceholderText(QStringLiteral("http://host:port"));
    connect(llama_edit_, &QLineEdit::textChanged, this,
            [this](const QString&) { emit settingsChanged(); });
    ai_form->addRow(QStringLiteral("llama.cpp server:"), llama_edit_);

    auto* test_row = new QHBoxLayout;
    test_button_ = new QPushButton(QStringLiteral("Test connection"));
    connect(test_button_, &QPushButton::clicked, this,
            &SettingsPanel::on_test_connection);
    test_status_ = new QLabel(QStringLiteral("Not tested yet."));
    test_status_->setWordWrap(true);
    test_row->addWidget(test_button_);
    test_row->addWidget(test_status_, 1);
    ai_form->addRow(QStringLiteral(""), test_row);
    layout->addWidget(ai_group);

    auto* launch_group = new QGroupBox(QStringLiteral("Backend launch"));
    auto* launch_form = new QFormLayout(launch_group);

    auto_launch_check_ = new QCheckBox(
        QStringLiteral("Auto-launch the backend when the app needs it and "
                       "nothing is listening"));
    connect(auto_launch_check_, &QCheckBox::toggled, this,
            [this](bool) { emit settingsChanged(); });
    launch_form->addRow(QStringLiteral(""), auto_launch_check_);

    launch_cmd_edit_ = new QLineEdit;
    launch_cmd_edit_->setPlaceholderText(
        QStringLiteral("e.g. ollama serve, or /path/to/llama-server -m model.gguf"));
    connect(launch_cmd_edit_, &QLineEdit::textChanged, this,
            [this](const QString&) { emit settingsChanged(); });
    launch_form->addRow(QStringLiteral("Launch command:"), launch_cmd_edit_);

    auto* dir_row = new QHBoxLayout;
    launch_dir_edit_ = new QLineEdit;
    launch_dir_edit_->setPlaceholderText(QStringLiteral(
        "Working directory for the launched process (optional)"));
    connect(launch_dir_edit_, &QLineEdit::textChanged, this,
            [this](const QString&) { emit settingsChanged(); });
    auto* browse = new QPushButton(QStringLiteral("Browse..."));
    connect(browse, &QPushButton::clicked, this, &SettingsPanel::on_browse_dir);
    dir_row->addWidget(launch_dir_edit_, 1);
    dir_row->addWidget(browse);
    launch_form->addRow(QStringLiteral("Working directory:"), dir_row);

    auto* launch_row = new QHBoxLayout;
    launch_button_ = new QPushButton(QStringLiteral("Launch now"));
    connect(launch_button_, &QPushButton::clicked, this,
            &SettingsPanel::on_launch_now);
    launch_status_ = new QLabel;
    launch_status_->setWordWrap(true);
    launch_row->addWidget(launch_button_);
    launch_row->addWidget(launch_status_, 1);
    launch_form->addRow(QStringLiteral(""), launch_row);
    layout->addWidget(launch_group);

    layout->addStretch(1);
}

SettingsPanel::~SettingsPanel() {
    while (worker_threads_.load() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

template <typename Fn>
void SettingsPanel::run_worker(Fn fn) {
    ++worker_threads_;
    std::thread([this, fn = std::move(fn)] {
        fn();
        --worker_threads_;
    }).detach();
}

ocs::llm::Config SettingsPanel::config() const {
    ocs::llm::Config c;
    c.ollama_url = ollama_edit_->text().trimmed().toStdString();
    c.llama_url = llama_edit_->text().trimmed().toStdString();
    c.launch_command = launch_cmd_edit_->text().trimmed().toStdString();
    c.launch_dir = launch_dir_edit_->text().trimmed().toStdString();
    c.auto_launch = auto_launch_check_->isChecked();
    return c;
}

void SettingsPanel::set_config(const ocs::llm::Config& cfg) {
    ollama_edit_->setText(QString::fromStdString(cfg.ollama_url));
    llama_edit_->setText(QString::fromStdString(cfg.llama_url));
    launch_cmd_edit_->setText(QString::fromStdString(cfg.launch_command));
    launch_dir_edit_->setText(QString::fromStdString(cfg.launch_dir));
    auto_launch_check_->setChecked(cfg.auto_launch);
}

util::Json SettingsPanel::snapshot() const {
    std::map<std::string, util::Json> m;
    m["ollama_url"] = util::Json(config().ollama_url);
    m["llama_url"] = util::Json(config().llama_url);
    m["ai_launch_cmd"] = util::Json(config().launch_command);
    m["ai_launch_dir"] = util::Json(config().launch_dir);
    m["ai_auto_launch"] = util::Json(config().auto_launch);
    return util::Json(std::move(m));
}

void SettingsPanel::restore(const util::Json& obj) {
    if (!obj.is_object()) return;
    if (obj.contains("ollama_url")) ollama_edit_->setText(
        QString::fromStdString(obj.at("ollama_url").as_string()));
    if (obj.contains("llama_url")) llama_edit_->setText(
        QString::fromStdString(obj.at("llama_url").as_string()));
    if (obj.contains("ai_launch_cmd")) launch_cmd_edit_->setText(
        QString::fromStdString(obj.at("ai_launch_cmd").as_string()));
    if (obj.contains("ai_launch_dir")) launch_dir_edit_->setText(
        QString::fromStdString(obj.at("ai_launch_dir").as_string()));
    if (obj.contains("ai_auto_launch")) auto_launch_check_->setChecked(
        obj.at("ai_auto_launch").as_bool());
}

void SettingsPanel::test_connection() {
    on_test_connection();
}

void SettingsPanel::on_test_connection() {
    if (testing_.load()) return;
    testing_.store(true);
    test_button_->setEnabled(false);
    test_status_->setText(QStringLiteral("Testing..."));
    run_worker([this] {
        const std::string status = ocs::llm::status_text();
        const bool found = !ocs::llm::backend_name().empty();
        QMetaObject::invokeMethod(this, [this, status, found] {
            test_status_->setText(
                found ? QStringLiteral("Connected: %1")
                            .arg(QString::fromStdString(status))
                      : QStringLiteral("No backend reachable. %1")
                            .arg(QString::fromStdString(status)));
            testing_.store(false);
            test_button_->setEnabled(true);
        }, Qt::QueuedConnection);
    });
}

void SettingsPanel::on_launch_now() {
    const ocs::llm::Config c = config();
    if (c.launch_command.empty()) {
        launch_status_->setText(QStringLiteral(
            "Enter a launch command first (e.g. \"ollama serve\")."));
        return;
    }
    // Apply the current fields first so the launcher sees them.
    ocs::llm::set_config(c);
    if (!ocs::llm::launch_backend()) {
        launch_status_->setText(QStringLiteral("Failed to start the process."));
        return;
    }
    launch_status_->setText(QStringLiteral(
        "Launched in the background (output: /tmp/ocs_ai_launch.log). "
        "Use \"Test connection\" to verify it responds."));
    // Refresh the probe cache; the backend may be reachable right away.
    ocs::llm::test_connection_reset();
    test_connection();
}

void SettingsPanel::on_browse_dir() {
    const QString dir = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Choose working directory"),
        launch_dir_edit_->text().isEmpty() ? QDir::homePath()
                                           : launch_dir_edit_->text());
    if (!dir.isEmpty()) launch_dir_edit_->setText(dir);
}

}  // namespace ocs::ui
