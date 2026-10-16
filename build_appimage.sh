#!/usr/bin/env bash
# Build an AppImage for OCS Studio
# Usage: ./build_appimage.sh
set -euo pipefail

VERSION="0.2.0"
APPNAME="ocs-studio"
ARCH="x86_64"

echo "=== Building OCS Studio AppImage v${VERSION} ==="

# Check dependencies
command -v cmake &>/dev/null || { echo "ERROR: cmake not found"; exit 1; }
command -nq linuxdeploy &>/dev/null || { echo "ERROR: linuxdeploy not found"; exit 1; }

# Clean previous build
rm -rf build-appimage
mkdir -p build-appimage

# Build
cd build-appimage
cmake .. -DCMAKE_BUILD_TYPE=Release -DOCS_TESTS=OFF -DCMAKE_INSTALL_PREFIX=/usr
cmake --build . -j$(nproc)

# Create AppDir structure
mkdir -p ${APPNAME}.AppDir/usr/{bin,lib,share/{applications,icons/hicolor/{16x16,32x32,48x48,64x64,128x128,256x256}/apps},share/metainfo}

# Copy binaries
cp ocs_studio ${APPNAME}.AppDir/usr/bin/
cp ocs_studio_gui ${APPNAME}.AppDir/usr/bin/

# Copy desktop file
cp ../ocs-studio.desktop ${APPNAME}.AppDir/

# Copy icons
cp ../assets/icons/ocs-studio-16x16.png ${APPNAME}.AppDir/usr/share/icons/hicolor/16x16/apps/
cp ../assets/icons/ocs-studio-32x32.png ${APPNAME}.AppDir/usr/share/icons/hicolor/32x32/apps/
cp ../assets/icons/ocs-studio-48x48.png ${APPNAME}.AppDir/usr/share/icons/hicolor/48x48/apps/
cp ../assets/icons/ocs-studio-64x64.png ${APPNAME}.AppDir/usr/share/icons/hicolor/64x64/apps/
cp ../assets/icons/ocs-studio-128x128.png ${APPNAME}.AppDir/usr/share/icons/hicolor/128x128/apps/
cp ../assets/icons/ocs-studio-256x256.png ${APPNAME}.AppDir/usr/share/icons/hicolor/256x256/apps/

# Create desktop entry in AppDir
cat > ${APPNAME}.AppDir/ocs-studio.desktop << 'DESKTOP'
[Desktop Entry]
Type=Application
Name=OCS Studio
Comment=Voice transformation toolkit: pitch shifting, gender conversion, virtual mic
Exec=ocs-studio.AppImage
Icon=ocs-studio
Terminal=false
Categories=Audio;AudioVideo;AudioEditing;
DESKTOP

# Run linuxdeploy
export LINUXDEPLOY_PLUGIN_APPIMAGE=1
linuxdeploy --appdir ${APPNAME}.AppDir \
    --desktop-file ${APPNAME}.AppDir/ocs-studio.desktop \
    --icon-file ../assets/icons/ocs-studio-256x256.png \
    --executable ${APPNAME}.AppDir/usr/bin/ocs_studio_gui \
    --output appimage

# Move AppImage to parent directory
mv ${APPNAME}-*.AppImage ../${APPNAME}-${VERSION}-${ARCH}.AppImage

echo ""
echo "=== AppImage built: ${APPNAME}-${VERSION}-${ARCH}.AppImage ==="
echo "Install: chmod +x ${APPNAME}-${VERSION}-${ARCH}.AppImage"
echo "Run: ./${APPNAME}-${VERSION}-${ARCH}.AppImage"
