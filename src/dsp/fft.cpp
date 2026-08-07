// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Open Cast Software Studio contributors
#include "dsp/fft.h"

#include <cmath>
#include <cstddef>
#include <utility>

namespace ocs::dsp {

size_t next_pow2(size_t n) {
    size_t p = 1;
    while (p < n) p <<= 1;
    return p;
}

void fft(std::vector<std::complex<double>>& data, bool inverse) {
    const size_t n = data.size();
    if (n <= 1) return;

    // Bit-reversal permutation.
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(data[i], data[j]);
    }

    const double sign = inverse ? 1.0 : -1.0;

    for (size_t len = 2; len <= n; len <<= 1) {
        const double ang = sign * 2.0 * M_PI / static_cast<double>(len);
        const std::complex<double> wlen(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> w(1.0, 0.0);
            for (size_t j = 0; j < len / 2; ++j) {
                const std::complex<double> u = data[i + j];
                const std::complex<double> v = data[i + j + len / 2] * w;
                data[i + j] = u + v;
                data[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }

    if (inverse) {
        for (auto& v : data) v /= static_cast<double>(n);
    }
}

}  // namespace ocs::dsp
