# Maintainer: OCS Studio Contributors <maintainer@ocs-studio.example>
# SPDX-License-Identifier: GPL-3.0-only

pkgname=ocs-studio
pkgver=0.2.0
pkgrel=1
pkgdesc='Open Cast Software Studio: voice transformation, gender conversion, virtual mic'
arch=(x86_64 aarch64)
url='https://github.com/bin4ort/open-cast-software-studio'
license=(GPL3)
depends=(
    qt6-base
    qt6-multimedia
    qt6-networkauth
    portaudio
    libsndfile
    fftw
    espeak-ng
    nlohmann-json
    curl
)
optdepends=(
    'ollama': AI voice correction and text rewriting
    'llama.cpp': AI voice correction and text rewriting
    'whisper-cpp': Speech-to-text transcription
    'ffmpeg': Fallback audio format conversion
)
source=("$pkgname::$url/archive/refs/tags/v$pkgver.tar.gz")
sha256sums=(
    SKIP  # Update with actual hash after tag push
)

prepare() {
    cd "$pkgname-$pkgver"
}

build() {
    cd "$pkgname-$pkgver"
    cmake -B build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DOCS_GUI=ON \
        -DOCS_TESTS=OFF
    cmake --build build -j$(nproc)
}

check() {
    cd "$pkgname-$pkgver"
    ctest --test-dir build --output-on-failure
}

package() {
    cd "$pkgname-$pkgver"

    # Install binaries
    cmake --install build --prefix "$pkgdir/usr"

    # Install license
    install -Dm644 LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"

    # Install desktop files
    install -Dm644 ocs-studio.desktop "$pkgdir/usr/share/applications/ocs-studio.desktop"
    install -Dm644 ocs_studio.desktop "$pkgdir/usr/share/applications/ocs_studio.desktop"

    # Install icons
    for size in 16 32 48 64 128 256; do
        install -Dm644 "assets/icons/ocs-studio-${size}x${size}.png" \
            "$pkgdir/usr/share/icons/hicolor/${size}x${size}/apps/ocs-studio.png"
    done

    # Install SVG icon (scalable)
    install -Dm644 assets/logo-framed.svg "$pkgdir/usr/share/icons/hicolor/scalable/apps/ocs-studio.svg"
}
