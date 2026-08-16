// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#pragma once

class QApplication;

namespace ocs::ui {

// Qt GUI entry point. Used by the standalone `ocs_studio_gui` binary and by
// the `ocs_studio gui` CLI subcommand (when built with OCS_HAS_GUI).
int gui_main(int argc, char** argv);

// Sets the application/window icon from the theme-appropriate logo variant
// (logo-dark.svg on dark system themes, logo-light.svg on light ones,
// falling back to logo.svg). No-op when the files are not found. Public for
// testing; called automatically by gui_main.
void apply_theme_icon(QApplication& app);

}  // namespace ocs::ui
