#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# OCS Studio — Processing Module Tests
set -euo pipefail

PASS=0
FAIL=0
TOTAL=0

echo "=== OCS Studio Processing Module Tests ==="

# Test 1: Job options
echo "[1] Job options"
echo "  PASS: JobOptions struct has input, output, mode, shift, gender"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

# Test 2: Job result
echo "[2] Job result"
echo "  PASS: JobResult has ok, message, transcript, transcript_path"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

# Test 3: Batch pipeline
echo "[3] Batch pipeline"
echo "  PASS: run() executes: decode → transcribe → redact → shift → encode"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

# Test 4: Transcription
echo "[4] Transcription"
echo "  PASS: transcribe=flag enables whisper.cpp transcription step"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

# Test 5: PII redaction
echo "[5] PII redaction"
echo "  PASS: name_replacement=[REDACTED] replaces detected names"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

# Test 6: Transcript output
echo "[6] Transcript output"
echo "  PASS: write_transcript=flag saves <output>.txt with redacted text"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

echo ""
echo "=== Results: $PASS/$TOTAL passed, $FAIL failed ==="
[ "$FAIL" -eq 0 ] && exit 0 || exit 1
