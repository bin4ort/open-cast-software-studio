// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QIcon>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QStandardPaths>
#include <QSvgRenderer>

#include "audio/device.h"
#include "ui/gui_main.h"
#include "ui/mainwindow.h"

namespace ocs::ui {

namespace {

// Candidate locations for the logo files: the installed share dir, or the
// source/build tree when running from a checkout.
QStringList logo_candidates(const QString& file) {
    QStringList candidates;
    for (const QString& base :
         QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation)) {
        candidates << base + QStringLiteral("/ocs_studio/") + file;
    }
    const QString app_dir = QCoreApplication::applicationDirPath();
    candidates << app_dir + QStringLiteral("/../share/ocs_studio/") + file
               << app_dir + QStringLiteral("/assets/") + file
               << QDir::currentPath() + QStringLiteral("/assets/") + file;
    return candidates;
}

// Returns a QIcon loaded from `file`, or a null icon when not found.
QIcon load_logo(const QString& file) {
    for (const QString& path : logo_candidates(file)) {
        if (!QFileInfo::exists(path)) continue;
        QSvgRenderer renderer(path);
        if (!renderer.isValid()) continue;
        QPixmap pm(renderer.defaultSize());
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        renderer.render(&p);
        p.end();
        return QIcon(pm);
    }
    return QIcon();
}

// Switches the icon variant when the system theme changes at runtime.
class ThemeFilter : public QObject {
public:
    using QObject::QObject;
protected:
    bool eventFilter(QObject* obj, QEvent* ev) override {
        if (ev->type() == QEvent::ApplicationPaletteChange) {
            if (auto* app = qobject_cast<QApplication*>(obj)) {
                apply_theme_icon(*app);
            }
        }
        return QObject::eventFilter(obj, ev);
    }
};

}  // namespace

// Picks the theme-appropriate logo variant and sets it as the application
// (window) icon: a dark-tinted framed mark for light system themes, a pale
// one for dark themes, falling back to the unframed variants.
void apply_theme_icon(QApplication& app) {
    const QPalette pal = app.palette();
    const int text_lightness = pal.color(QPalette::WindowText).lightness();
    const bool dark_theme = text_lightness > 128;
    QIcon icon = load_logo(dark_theme ? QStringLiteral("logo-framed-dark.svg")
                                      : QStringLiteral("logo-framed-light.svg"));
    if (icon.isNull()) icon = load_logo(QStringLiteral("logo-framed.svg"));
    if (icon.isNull()) icon = load_logo(QStringLiteral("logo.svg"));
    if (!icon.isNull()) app.setWindowIcon(icon);
}

int gui_main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setApplicationName("OCS Studio");
    app.setOrganizationName("OCS Studio");
    apply_theme_icon(app);
    // Follow the system theme at runtime (icon variant switches on
    // light/dark palette changes).
    app.installEventFilter(new ThemeFilter(&app));
    // Device enumeration on the live tab needs PortAudio initialized.
    // A failure is not fatal: the GUI still runs, just without live audio.
    ocs::audio::pa_init();
    ocs::ui::MainWindow window;
    window.show();
    const int rc = app.exec();
    ocs::audio::pa_shutdown();
    return rc;
}

}  // namespace ocs::ui
