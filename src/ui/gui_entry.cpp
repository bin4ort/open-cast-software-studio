// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#include "ui/gui_main.h"

// Standalone GUI binary entry point; the same `ocs::ui::gui_main` is reused
// by the `ocs_studio gui` CLI subcommand.
int main(int argc, char** argv) {
    return ocs::ui::gui_main(argc, argv);
}
