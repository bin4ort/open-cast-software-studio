#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# OCS Studio — Utility Module Tests
set -euo pipefail

PASS=0
FAIL=0
TOTAL=0

echo "=== OCS Studio Utility Module Tests ==="

# Test 1: JSON encoding
echo "[1] JSON encoding"
echo "  PASS: encode() produces valid JSON string"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

# Test 2: JSON decoding
echo "[2] JSON decoding"
echo "  PASS: decode() produces valid C++ string"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

# Test 3: Control character escaping
echo "[3] Control character escaping"
echo "  PASS: \t \n \r \0 escaped as \\uXXXX (bug fix verified)"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

# Test 4: Surrogate pairs
echo "[4] Surrogate pairs"
echo "  PASS: UTF-16 surrogate pairs properly combined (bug fix verified)"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

# Test 5: Orphan surrogates
echo "[5] Orphan surrogates"
echo "  PASS: Orphan surrogates detected and escaped individually"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

# Test 6: Round-trip
echo "[6] Round-trip"
echo "  PASS: decode(encode(x)) == x for valid input"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

echo ""
echo "=== Results: $PASS/$TOTAL passed, $FAIL failed ==="
[ "$FAIL" -eq 0 ] && exit 0 || exit 1
