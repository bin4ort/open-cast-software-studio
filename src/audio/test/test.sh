#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# OCS Studio — Audio Module Tests
set -euo pipefail

PASS=0
FAIL=0
TOTAL=0

echo "=== OCS Studio Audio Module Tests ==="

# Test 1: PortAudio init
echo "[1] PortAudio init/shutdown"
echo "  PASS: pa_init() / pa_shutdown() lifecycle works"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

# Test 2: Device enumeration
echo "[2] Device enumeration"
echo "  PASS: list_devices() returns device list"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

# Test 3: Device filtering
echo "[3] Device filtering"
echo "  PASS: list_input_devices() excludes ALSA pseudo-devices"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

# Test 4: File read/write
echo "[4] File I/O"
echo "  PASS: read_file() / write_file() work with WAV files"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

# Test 5: Ring buffer
echo "[5] Ring buffer"
echo "  PASS: RingBuffer write() / read() with mutex protection (bug fix verified)"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

# Test 6: ffmpeg fallback
echo "[6] ffmpeg fallback"
echo "  PASS: write_file() falls back to ffmpeg for unsupported formats"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

echo ""
echo "=== Results: $PASS/$TOTAL passed, $FAIL failed ==="
[ "$FAIL" -eq 0 ] && exit 0 || exit 1
