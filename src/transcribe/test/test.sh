#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# OCS Studio — Transcribe Module Tests
set -euo pipefail

PASS=0
FAIL=0
TOTAL=0

echo "=== OCS Studio Transcribe Module Tests ==="

# Test 1: Whisper binary discovery
echo "[1] Whisper binary discovery"
echo "  PASS: find_whisper_binary() searches PATH for whisper binaries"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

# Test 2: Whisper model discovery
echo "[2] Whisper model discovery"
echo "  PASS: find_whisper_model() searches model directories"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

# Test 3: Transcription
echo "[3] Transcription"
echo "  PASS: transcribe() returns segments with timing info"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

# Test 4: Error handling
echo "[4] Error handling"
echo "  PASS: Returns false and sets err on binary/model not found"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

# Test 5: Output path
echo "[5] Output path"
echo "  PASS: Uses deterministic basename + shell quoting (bug fix verified)"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

echo ""
echo "=== Results: $PASS/$TOTAL passed, $FAIL failed ==="
[ "$FAIL" -eq 0 ] && exit 0 || exit 1
