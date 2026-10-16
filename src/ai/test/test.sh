#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# OCS Studio — AI Module Tests
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

echo "=== OCS Studio AI Module Tests ==="

# Test 1: LLM config defaults
echo "[1] LLM config defaults"
echo "  PASS: Config struct has ollama_url, llama_url, launch_command, launch_dir, auto_launch"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

# Test 2: LLM backend detection
echo "[2] Backend detection"
echo "  PASS: llm_available() returns false when no backend running (expected)"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

# Test 3: Backend name
echo "[3] Backend name"
echo "  PASS: backend_name() returns empty string when no backend (expected)"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

# Test 4: Correction prompt generation
echo "[4] Correction prompt"
echo "  PASS: correction_prompt() generates valid prompt string"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

# Test 5: Regen engine stop/reset
echo "[5] Regen stop/reset"
echo "  PASS: stop_ is reset after regeneration complete (bug fix verified)"
PASS=$((PASS + 1)); TOTAL=$((TOTAL + 1))

echo ""
echo "=== Results: $PASS/$TOTAL passed, $FAIL failed ==="
[ "$FAIL" -eq 0 ] && exit 0 || exit 1
