// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 voice_anonymizer contributors
#include <QApplication>

#include "audio/device.h"
#include "ui/mainwindow.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setApplicationName("voice_anonymizer");
    app.setOrganizationName("voice_anonymizer");
    // Device enumeration on the live tab needs PortAudio initialized.
    va::audio::pa_init();
    va::ui::MainWindow window;
    window.show();
    const int rc = app.exec();
    va::audio::pa_shutdown();
    return rc;
}
