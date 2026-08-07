// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#include <QApplication>

#include "audio/device.h"
#include "ui/mainwindow.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setApplicationName("OCS Studio");
    app.setOrganizationName("OCS Studio");
    // Device enumeration on the live tab needs PortAudio initialized.
    ocs::audio::pa_init();
    ocs::ui::MainWindow window;
    window.show();
    const int rc = app.exec();
    ocs::audio::pa_shutdown();
    return rc;
}
