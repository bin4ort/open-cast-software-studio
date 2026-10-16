#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# OCS Studio — Quick install script
# Usage: ./install.sh [--prefix /usr/local] [--uninstall]
set -euo pipefail

PREFIX="$HOME/.local"
UNINSTALL=false

for arg in "$@"; do
    case "$arg" in
        --uninstall)
            UNINSTALL=true
            ;;
        --prefix)
            PREFIX="$2"
            shift
            ;;
    esac
done

if $UNINSTALL; then
    echo "=== Uninstalling OCS Studio ==="
    rm -f "$PREFIX/bin/ocs_studio"
    rm -f "$PREFIX/bin/ocs_studio_gui"
    rm -f "$HOME/.local/share/applications/ocs-studio.desktop"
    rm -f "$HOME/.local/share/applications/ocs_studio.desktop"
    echo "Done."
    exit 0
fi

echo "=== Installing OCS Studio to $PREFIX ==="

# Check if binaries exist
if [[ ! -f build/ocs_studio || ! -f build/ocs_studio_gui ]]; then
    echo "Building first..."
    cmake -B build -DCMAKE_BUILD_TYPE=Release -DOCS_TESTS=OFF
    cmake --build build -j$(nproc)
fi

# Create directories
mkdir -p "$PREFIX/bin"
mkdir -p "$HOME/.local/share/applications"
mkdir -p "$HOME/.local/share/icons/hicolor/scalable/apps"

# Copy binaries
cp build/ocs_studio "$PREFIX/bin/"
cp build/ocs_studio_gui "$PREFIX/bin/"

# Install desktop files
cat > "$HOME/.local/share/applications/ocs-studio.desktop" << 'DESKTOP'
[Desktop Entry]
Type=Application
Name=OCS Studio
Comment=Voice transformation toolkit: pitch shifting, gender conversion, virtual mic
Exec=ocs_studio_gui
Icon=ocs-studio
Terminal=false
Categories=Audio;AudioVideo;AudioEditing;
DESKTOP

cat > "$HOME/.local/share/applications/ocs_studio.desktop" << 'DESKTOP'
[Desktop Entry]
Type=Application
Name=OCS Studio CLI
Comment=Voice transformation toolkit: pitch shifting, gender conversion, virtual mic
Exec=ocs_studio %F
Terminal=true
Categories=Audio;AudioVideo;AudioEditing;
DESKTOP

# Copy icons
cp assets/logo-framed.svg "$HOME/.local/share/icons/hicolor/scalable/apps/ocs-studio.svg"
for size in 16 32 48 64 128 256; do
    if [[ -f "assets/icons/ocs-studio-${size}x${size}.png" ]]; then
        mkdir -p "$HOME/.local/share/icons/hicolor/${size}x${size}/apps"
        cp "assets/icons/ocs-studio-${size}x${size}.png" "$HOME/.local/share/icons/hicolor/${size}x${size}/apps/ocs-studio.png"
    fi
done

echo "=== Installed ==="
echo "  CLI: $PREFIX/bin/ocs_studio"
echo "  GUI: $PREFIX/bin/ocs_studio_gui"
echo "  Desktop: $HOME/.local/share/applications/ocs-studio.desktop"
echo "  Icons: $HOME/.local/share/icons/hicolor/"
echo ""
echo "To launch from desktop: Application menu -> Audio -> OCS Studio"
echo "To uninstall: $0 --uninstall"
