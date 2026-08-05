// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 voice_anonymizer contributors
#pragma once

#include <complex>
#include <vector>

namespace va::dsp {

// In-place iterative radix-2 Cooley-Tukey FFT.
// `n` must be a power of two and `data` sized exactly `n`.
//
// inverse == false : FFT.  data[k] = sum_j x[j] * exp(-2*pi*i*j*k/n)
// inverse == true  : IFFT. data[j] = sum_k X[k] * exp(+2*pi*i*j*k/n) / n
void fft(std::vector<std::complex<double>>& data, bool inverse);

inline bool is_power_of_two(size_t n) { return n != 0 && (n & (n - 1)) == 0; }

// Returns the next power of two >= n.
size_t next_pow2(size_t n);

}  // namespace va::dsp
