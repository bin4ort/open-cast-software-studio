# OCS Studio — DSP Module Standard

<!-- SPDX-License-Identifier: GPL-3.0-only -->

## Purpose

Digital signal processing: pitch/formant shifting, time stretching, denoising, FFT, audio comparison.

## Module Root

`src/dsp/`

## Source Files

- `fft.h` / `fft.cpp` — Radix-2 Cooley-Tukey FFT
- `gender.h` / `gender.cpp` — Pitch + formant presets
- `pipeline.h` / `pipeline.cpp` — ShiftParams, processing config
- `phase_vocoder.h` / `phase_vocoder.cpp` — Phase vocoder (hop_=0 guarded)
- `vocoder.h` / `vocoder.cpp` — Vocal tract synthesis (TTS)
- `compare.h` / `compare.cpp` — Audio difference analysis
- `denoise.h` / `denoise.cpp` — Spectral subtraction denoiser

## Dependencies

- FFTW3 (libfftw3)
- espeak-ng (TTS)
- `common.h` (AudioData)

## Public API

| Function | Namespace | Purpose |
|----------|-----------|---------|
| `shift_voice()` | `ocs::dsp` | Pitch + formant shift |
| `time_stretch()` | `ocs::dsp` | Time stretch (no pitch change) |
| `pitch_shift()` | `ocs::dsp` | Pitch shift (no time change) |
| `analyze_diff()` | `ocs::dsp` | Compare original vs processed |
| `denoise()` | `ocs::dsp` | Spectral denoising |

## Test File

`src/dsp/test/test.sh`

## License

GPL-3.0-only
