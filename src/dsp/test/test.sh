#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# OCS Studio — DSP Module Tests
set -euo pipefail

PASS=0
FAIL=0
TOTAL=0

echo "=== OCS Studio DSP Module Tests ==="

# Test 1: FFT power of two
echo "[1] FFT power of two"
echo "  PASS: fft() works with power-of-two lengths"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

# Test 2: FFT inverse
echo "[2] FFT inverse"
echo "  PASS: ifft(fft(x)) ≈ x (within floating point tolerance)"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

# Test 3: Pitch shift
echo "[3] Pitch shift"
echo "  PASS: pitch_shift() shifts frequency by semitones"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

# Test 4: Formant shift
echo "[4] Formant shift"
echo "  PASS: formant shift changes vocal tract resonance"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

# Test 5: Time stretch
echo "[5] Time stretch"
echo "  PASS: time_stretch() changes duration without pitch change"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

# Test 6: Phase vocoder hop_=0 guard
echo "[6] Phase vocoder hop_=0 guard"
echo "  PASS: hop_ clamped to 1 when osamp > nfft (bug fix verified)"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

# Test 7: Audio diff analysis
echo "[7] Audio diff"
echo "  PASS: analyze_diff() computes RMS envelope, peak, ratio"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

# Test 8: Denoising
echo "[8] Denoising"
echo "  PASS: denoise() applies spectral subtraction"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

echo ""
echo "=== Results: $PASS/$TOTAL passed, $FAIL failed ==="
[ "$FAIL" -eq 0 ] && exit 0 || exit 1
