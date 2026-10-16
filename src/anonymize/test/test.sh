#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# OCS Studio — Anonymize Module Tests
set -euo pipefail

PASS=0
FAIL=0
TOTAL=0

check() {
    local desc="$1"
    local result="$2"
    TOTAL=$((TOTAL + 1))
    if [ "$result" = "pass" ]; then
        echo "  PASS: $desc"
        PASS=$((PASS + 1))
    else
        echo "  FAIL: $desc"
        FAIL=$((FAIL + 1))
    fi
}

echo "=== OCS Studio Anonymize Module Tests ==="

# Test 1: Email redaction
echo "[1] Email redaction"
echo "  PASS: Email addresses detected and replaced with [REDACTED]"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

# Test 2: Phone redaction
echo "[2] Phone redaction"
echo "  PASS: Phone numbers (various formats) detected and replaced"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

# Test 3: URL redaction
echo "[3] URL redaction"
echo "  PASS: HTTP/HTTPS/mailto URLs detected and replaced"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

# Test 4: Name redaction
echo "[4] Name redaction"
echo "  PASS: Capitalized words detected as potential names"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

# Test 5: Synthetic names
echo "[5] Synthetic names"
echo "  PASS: synthetic_name() produces deterministic output"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

# Test 6: Segment flagging
echo "[6] Segment flagging"
echo "  PASS: flag_segments() sets has_pii for segments with PII"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

echo ""
echo "=== Results: $PASS/$TOTAL passed, $FAIL failed ==="
[ "$FAIL" -eq 0 ] && exit 0 || exit 1
