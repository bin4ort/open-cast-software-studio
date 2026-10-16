# OCS Studio — DSP Module

<!-- SPDX-License-Identifier: GPL-3.0-only -->
<!-- Copyright (C) 2026 Open Cast Software Studio contributors -->

## Overview

The `dsp` module implements digital signal processing algorithms for voice transformation: pitch shifting, formant shifting, time stretching, spectral denoising, FFT, and audio comparison for AI correction.

## Components

| File | Description |
|------|-------------|
| `fft.h/cpp` | Radix-2 Cooley-Tukey FFT and IFFT (in-place, power-of-two length) |
| `gender.h/cpp` | Pitch + formant shifting presets (Generic/Male/Female) |
| `pipeline.h/cpp` | Processing pipeline configuration (ShiftParams, Gender enum) |
| `phase_vocoder.h/cpp` | Phase vocoder for time-stretching and pitch shifting |
| `vocoder.h/cpp` | Vocal tract synthesis (TTS backend) |
| `compare.h/cpp` | Audio difference analysis (RMS envelope, peak, aligned comparison) |
| `denoise.h/cpp` | Spectral subtraction denoiser |

## Dependencies

- FFTW3 (libfftw3) — FFT computation
- espeak-ng — Vocal tract synthesis / TTS
- `common.h` — AudioData struct definition

## API

```cpp
namespace ocs::dsp {
    enum class Gender { Generic, Male, Female };

    struct ShiftParams {
        double pitch = 1.0;     // semitones
        double formant = 1.0;   // formant shift
        double time_scale = 1.0; // time stretch factor
    };

    // Pitch + formant shift (phase vocoder based)
    AudioData shift_voice(const AudioData& input, const ShiftParams& params);

    // Time stretch without pitch change
    AudioData time_stretch(const AudioData& input, double factor);

    // Pitch shift without time change
    AudioData pitch_shift(const AudioData& input, double semitones);

    // Analyze difference between original and processed audio
    AudioDiff analyze_diff(const std::vector<float>& orig,
                           const std::vector<float>& proc, size_t buckets = 128);

    // Spectral denoising
    std::vector<double> denoise(const std::vector<double>& input,
                                const DenoiseParams& params = {});
}
```

## Phase Vocoder

The phase vocoder uses overlapping FFT/IFFT with configurable hop size. Guard against `hop_ == 0` (causes infinite loop when `osamp > nfft`).

## Testing

```bash
bash src/dsp/test/test.sh
```

## License

GPL-3.0-only. See [../../LICENSE](../../LICENSE) for details.
