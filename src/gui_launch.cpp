// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#include "ui/gui_main.h"

// Strong definition of the weak symbol declared in cli.cpp. Compiled into the
// `ocs_studio` executable only when the Qt GUI is available, so the `gui`
// subcommand works. GUI-less builds keep the weak symbol null and the CLI
// reports that the GUI is unavailable.
extern "C" int ocs_gui_launch(int argc, char** argv) {
    return ocs::ui::gui_main(argc, argv);
}
