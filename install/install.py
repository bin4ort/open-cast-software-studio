#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Open Cast Software Studio contributors
"""
OCS Studio (Open Cast Software Studio) CLI install wizard.

Interactive installer that:
  1. Shows and requires acceptance of the GPL-3.0 license.
  2. Detects the distro and checks build/runtime dependencies.
  3. Lets the user either auto-install missing packages (sudo, with a
     preview of the exact commands) or install them manually.
  4. Builds and installs the binary (+ optional GUI .desktop entry).
  5. Supports uninstall via a manifest.

Python 3 only, no third-party modules.
"""

from __future__ import annotations

import os
import platform
import re
import shutil
import subprocess
import sys
from pathlib import Path

PROJECT = "ocs_studio"
VERSION = "0.2.0"
CLI_BIN = "ocs_studio"
GUI_BIN = "ocs_studio_gui"

GPL_SUMMARY = f"""\
{PROJECT} {VERSION} is free software, licensed under the GNU General Public
License, version 3 (GPL-3.0-only). This license gives you the freedom to use,
study, share and modify the program, provided any derivative works you
distribute are licensed under the same terms and include the license text and
source code.

You should have received a copy of the license with the source (LICENSE file).
If not, see <https://www.gnu.org/licenses/>.
"""


# ---------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------
def say(msg: str = "") -> None:
    print(msg)


def ask(prompt: str, default: str = "") -> str:
    suffix = f" [{default}]" if default else ""
    try:
        return input(f"{prompt}{suffix}: ").strip() or default
    except (EOFError, KeyboardInterrupt):
        print()
        sys.exit(1)


def confirm(prompt: str, default: bool = True) -> bool:
    hint = "[Y/n]" if default else "[y/N]"
    answer = ask(prompt, hint).lower()
    if not answer:
        return default
    return answer in ("y", "yes")


def run(cmd: list[str], check: bool = True, capture: bool = False) -> subprocess.CompletedProcess:
    if not capture:
        print("  $ " + " ".join(cmd))
    try:
        return subprocess.run(
            cmd, check=check, capture_output=capture, text=True, env=os.environ.copy()
        )
    except FileNotFoundError:
        say(f"  error: command not found: {cmd[0]}")
        sys.exit(1)


def cmd_exists(name: str) -> bool:
    return shutil.which(name) is not None


def pkg_exists(pc: str) -> bool:
    return cmd_exists("pkg-config") and run(
        ["pkg-config", "--exists", pc], check=False, capture=True
    ).returncode == 0


def is_root() -> bool:
    return hasattr(os, "geteuid") and os.geteuid() == 0


def sudo_prefix() -> list[str]:
    return [] if is_root() else ["sudo"]


# ---------------------------------------------------------------------------
# dependency checks
# ---------------------------------------------------------------------------
class Dep:
    def __init__(self, name: str, check, packages: dict[str, str], hint: str = ""):
        self.name = name
        self.check = check
        self.packages = packages  # pmgr -> package(s)
        self.hint = hint

    def ok(self) -> bool:
        try:
            return bool(self.check())
        except Exception:
            return False


def check_compiler() -> bool:
    if not cmd_exists("g++"):
        return False
    out = run(["g++", "--version"], check=False, capture=True).stdout
    m = re.search(r"g\+\+ \(.*\) (\d+)\.", out)
    return bool(m and int(m.group(1)) >= 11)


def check_cmake() -> bool:
    if not cmd_exists("cmake"):
        return False
    out = run(["cmake", "--version"], check=False, capture=True).stdout
    m = re.search(r"cmake version (\d+)\.(\d+)", out)
    return bool(m and (int(m.group(1)), int(m.group(2))) >= (3, 20))


def check_qt() -> bool:
    return pkg_exists("Qt6Widgets") or os.path.isdir("/usr/include/qt6") or os.path.isdir(
        "/usr/lib/qt6"
    )


# Dependencies keyed by package manager.
DEPS = [
    Dep(
        "g++ (C++20)",
        check_compiler,
        {"apt": "build-essential", "dnf": "gcc-c++", "pacman": "base-devel", "zypper": "gcc-c++"},
    ),
    Dep(
        "cmake >= 3.20",
        check_cmake,
        {"apt": "cmake", "dnf": "cmake", "pacman": "cmake", "zypper": "cmake"},
    ),
    Dep(
        "make",
        lambda: cmd_exists("make"),
        {"apt": "build-essential", "dnf": "make", "pacman": "base-devel", "zypper": "make"},
    ),
    Dep(
        "PortAudio",
        lambda: pkg_exists("portaudio-2.0"),
        {"apt": "libportaudio-dev", "dnf": "portaudio-devel", "pacman": "portaudio", "zypper": "portaudio-devel"},
    ),
    Dep(
        "libsndfile",
        lambda: pkg_exists("sndfile"),
        {"apt": "libsndfile1-dev", "dnf": "libsndfile-devel", "pacman": "libsndfile", "zypper": "libsndfile-devel"},
    ),
    Dep(
        "libsamplerate",
        lambda: pkg_exists("samplerate"),
        {"apt": "libsamplerate0-dev", "dnf": "libsamplerate-devel", "pacman": "libsamplerate", "zypper": "libsamplerate-devel"},
    ),
    Dep(
        "ffmpeg (optional, extra formats)",
        lambda: cmd_exists("ffmpeg"),
        {"apt": "ffmpeg", "dnf": "ffmpeg", "pacman": "ffmpeg", "zypper": "ffmpeg"},
    ),
    Dep(
        "Qt6 Widgets dev (GUI)",
        check_qt,
        {"apt": "qt6-base-dev", "dnf": "qt6-qtbase-devel", "pacman": "qt6-base", "zypper": "qt6-base-devel"},
    ),
]

# Optional, feature-enabling dependencies.
OPTIONAL_DEPS = [
    Dep(
        "whisper-cpp (transcription)",
        lambda: cmd_exists("whisper-cli") or cmd_exists("whisper") or cmd_exists("main"),
        {"apt": "whisper-cpp", "dnf": "whisper-cpp", "pacman": "whisper-cpp", "zypper": "whisper-cpp"},
    ),
    Dep(
        "espeak-ng (voice preview TTS)",
        lambda: cmd_exists("espeak-ng"),
        {"apt": "espeak-ng", "dnf": "espeak-ng", "pacman": "espeak-ng", "zypper": "espeak-ng"},
    ),
    Dep(
        "onnxruntime-cpu (experimental AI voice conversion)",
        lambda: pkg_exists("onnxruntime"),
        {"apt": "libonnxruntime-dev", "dnf": "onnxruntime", "pacman": "onnxruntime-cpu", "zypper": "onnxruntime"},
    ),
]


def detect_pmgr() -> str | None:
    if cmd_exists("apt-get"):
        return "apt"
    if cmd_exists("dnf"):
        return "dnf"
    if cmd_exists("pacman"):
        return "pacman"
    if cmd_exists("zypper"):
        return "zypper"
    return None


def check_all() -> tuple[list[Dep], list[Dep]]:
    ok, missing = [], []
    for d in DEPS:
        (ok if d.ok() else missing).append(d)
    return ok, missing


def describe(deps: list[Dep]) -> None:
    for d in deps:
        say(f"  [{'OK' if d.ok() else 'MISSING'}] {d.name}")


# ---------------------------------------------------------------------------
# install steps
# ---------------------------------------------------------------------------
def step_license() -> None:
    say(GPL_SUMMARY)
    if not confirm("Do you accept the GPL-3.0 license terms?"):
        say("Installation aborted - license not accepted.")
        sys.exit(1)
    say()


def step_dependencies() -> None:
    ok, missing = check_all()
    say(f"Detected package manager: {detect_pmgr() or 'unknown'}")
    say("Dependency status:")
    describe(ok)
    describe(missing)
    say()

    if not missing:
        say("All dependencies are already satisfied.")
        return

    pmgr = detect_pmgr()
    if not pmgr:
        say("Could not auto-detect a package manager.")
        say("Install the missing packages manually, then rerun this wizard.")
        sys.exit(1)

    choice = ask(
        "Install missing dependencies?\n"
        "  [1] Auto-install via package manager (sudo)\n"
        "  [2] Manual - I will install them myself",
        "1",
    )
    if choice != "1":
        say("Manual mode selected. Installing deps...")
        ok2, missing2 = check_all()
        if not missing2:
            say("All dependencies are now present (installed in another terminal?).")
            return
        say()
        say("The following are still missing:")
        for d in missing2:
            say(f"  - {d.name}: {d.hint or d.packages.get(pmgr, 'unknown package')}")
        say()
        say("Please install them, then re-run this wizard to continue the build.")
        sys.exit(0)  # clean exit, user must rerun after installing deps

    # Auto mode: preview then install missing packages.
    pkgs = []
    for d in missing:
        p = d.packages.get(pmgr)
        if p:
            pkgs.extend(p.split())
    if not pkgs:
        say("Nothing to install.")
        return
    pkgs = sorted(set(pkgs))
    install_pkgs(pkgs, pmgr)

    ok3, missing3 = check_all()
    if missing3:
        say()
        say("Some dependencies could not be verified after install:")
        for d in missing3:
            say(f"  - {d.name}")
        if not confirm("Continue anyway?"):
            sys.exit(1)
    say()


def install_pkgs(pkgs: list[str], pmgr: str) -> None:
    if pmgr == "apt":
        update = sudo_prefix() + ["apt-get", "update"]
        install = sudo_prefix() + ["apt-get", "install", "-y"] + pkgs
    elif pmgr == "dnf":
        update, install = [], sudo_prefix() + ["dnf", "install", "-y"] + pkgs
    elif pmgr == "pacman":
        update = sudo_prefix() + ["pacman", "-Sy"]
        install = sudo_prefix() + ["pacman", "-S", "--noconfirm", "--needed"] + pkgs
    else:  # zypper
        update, install = [], sudo_prefix() + ["zypper", "install", "-y"] + pkgs

    say()
    say("Commands that will be run:")
    for cmd in ([update] if update else []) + [install]:
        say("  $ " + " ".join(cmd))
    say()
    if not confirm("Run these with sudo now?"):
        say("Aborted. Run the wizard again when ready.")
        sys.exit(1)

    if update:
        run(update)
    run(install)
    say()


def step_optional_dependencies() -> None:
    missing = [d for d in OPTIONAL_DEPS if not d.ok()]
    if not missing:
        return
    say("Optional features are missing their runtime tools:")
    for d in missing:
        say(f"  - {d.name}")
    say()
    pmgr = detect_pmgr()
    if not pmgr:
        say("Install them manually if you want the corresponding features "
            "(transcription, voice preview, experimental AI voice conversion).")
        say()
        return
    if not confirm("Install the optional tools now (sudo)?", default=False):
        say("Skipping optional tools. The app will build without them.")
        say()
        return
    pkgs = sorted({
        p for d in missing for p in d.packages.get(pmgr, "").split() if p
    })
    if pkgs:
        install_pkgs(pkgs, pmgr)
    say()


def step_build() -> str:
    say("Configuring with CMake...")
    build_dir = Path.cwd() / "build"
    run(["cmake", "-B", str(build_dir), "-S", str(Path.cwd()),
         "-DCMAKE_BUILD_TYPE=Release"])
    say("Building...")
    jobs = str(os.cpu_count() or 2)
    run(["cmake", "--build", str(build_dir), "-j", jobs])
    if not (build_dir / CLI_BIN).exists():
        say("error: build did not produce the binary")
        sys.exit(1)
    return str(build_dir)


def step_install(build_dir: str) -> None:
    system = confirm("Install system-wide to /usr/local (requires sudo)?", default=False)
    prefix = "/usr/local" if system else str(Path.home() / ".local")
    say(f"Install prefix: {prefix}")
    if not confirm("Proceed with install?"):
        sys.exit(1)

    pre = sudo_prefix() if system else []
    run(pre + ["cmake", "--install", build_dir, "--prefix", prefix])

    # Persist the manifest for uninstall even if the build dir is removed.
    # For system installs the share dir needs root, so create/copy via sudo.
    manifest_src = Path(build_dir) / "install_manifest.txt"
    manifest_dst = Path(prefix) / "share" / PROJECT
    if manifest_src.exists():
        if system and not is_root():
            run(["sudo", "mkdir", "-p", str(manifest_dst)])
            run(["sudo", "cp", str(manifest_src),
                 str(manifest_dst / "install_manifest.txt")])
            run(["sudo", "chown", "-R", f"{os.getuid()}:{os.getgid()}",
                 str(manifest_dst)], check=False)
        else:
            manifest_dst.mkdir(parents=True, exist_ok=True)
            shutil.copy(manifest_src, manifest_dst / "install_manifest.txt")

    if cmd_exists(GUI_BIN) or (Path(build_dir) / GUI_BIN).exists():
        if confirm("Create a desktop menu entry for the GUI?", default=True):
            gui_bin = str(Path(prefix) / "bin" / GUI_BIN)
            desktop = (
                f"[Desktop Entry]\nType=Application\nName={PROJECT}\n"
                f"Comment=Voice anonymization toolkit\n"
                f"Exec={gui_bin}\nTerminal=false\nCategories=AudioVideo;Audio;\n"
            )
            entries = Path.home() / ".local/share/applications"
            entries.mkdir(parents=True, exist_ok=True)
            (entries / f"{PROJECT}.desktop").write_text(desktop)
            say(f"  desktop entry: {entries / (PROJECT + '.desktop')}")

    say()
    say("Installation complete.")
    say(f"  CLI: {Path(prefix) / 'bin' / CLI_BIN}")
    if (Path(build_dir) / GUI_BIN).exists():
        say(f"  GUI: {Path(prefix) / 'bin' / GUI_BIN}")


def step_uninstall() -> None:
    say("Uninstall")
    targets = ["/usr/local", str(Path.home() / ".local")]
    choice = ask("Uninstall from:\n  [1] /usr/local (system)\n  [2] ~/.local (user)", "2")
    prefix = targets[0] if choice == "1" else targets[1]

    manifest = Path(prefix) / "share" / PROJECT / "install_manifest.txt"
    if not manifest.exists():
        say(f"No manifest found at {manifest}.")
        say("Remove the files manually (the binaries live in "
            f"{Path(prefix) / 'bin'}).")
        sys.exit(1)

    files = [line.strip() for line in manifest.read_text().splitlines() if line.strip()]
    pre = sudo_prefix() if prefix == "/usr/local" else []
    removed = 0
    for f in files:
        p = Path(f)
        if p.exists():
            run(pre + ["rm", "-f", str(p)], check=False)
            removed += 1
    if manifest.exists():
        run(pre + ["rm", "-f", str(manifest)], check=False)

    desktop = Path.home() / ".local/share/applications" / f"{PROJECT}.desktop"
    if desktop.exists():
        desktop.unlink()

    say(f"Removed {removed} file(s). Done.")


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------
def main() -> None:
    args = sys.argv[1:]
    if args and args[0] == "uninstall":
        step_uninstall()
        return

    say(f"OCS Studio (Open Cast Software Studio) {VERSION} - CLI install wizard (GPL-3.0-only)")
    say("=" * 60)
    say()
    step_license()
    step_dependencies()
    step_optional_dependencies()
    build_dir = step_build()
    step_install(build_dir)


if __name__ == "__main__":
    main()
