#!/bin/bash
#
# test_uthread.sh - Automated tests for the user-level thread library
#
# Usage: ./test_uthread.sh [path_to_source_dir]
#   path_to_source_dir: directory containing uthread.c, uthread.h, main.c
#                       defaults to ../skeleton

set -e

SRCDIR="${1:-../skeleton}"
TESTDIR="$(cd "$(dirname "$0")" && pwd)"
TMPDIR=$(mktemp -d)
trap "rm -rf $TMPDIR" EXIT

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No color

PASS=0
FAIL=0

pass() {
    echo -e "${GREEN}PASS${NC}: $1"
    PASS=$((PASS + 1))
}

fail() {
    echo -e "${RED}FAIL${NC}: $1"
    FAIL=$((FAIL + 1))
}

# ============================================================
# Test 1: Compilation
# ============================================================
echo "=== Test 1: Compilation ==="

if gcc -Wall -Werror -o "$TMPDIR/uthread" \
    "$SRCDIR/uthread.c" "$SRCDIR/main.c" -I"$SRCDIR" 2>"$TMPDIR/compile_err.txt"; then
    pass "Compiles without warnings or errors"
else
    fail "Compilation failed"
    echo "  Compiler output:"
    sed 's/^/    /' "$TMPDIR/compile_err.txt"
    echo ""
    echo "Cannot continue without a successful build."
    exit 1
fi

# ============================================================
# Test 2: Program runs and exits cleanly
# ============================================================
echo ""
echo "=== Test 2: Execution ==="

if timeout 10 "$TMPDIR/uthread" > "$TMPDIR/output.txt" 2>&1; then
    pass "Program runs and exits with code 0"
else
    EXIT_CODE=$?
    if [ $EXIT_CODE -eq 124 ]; then
        fail "Program timed out (possible infinite loop)"
    else
        fail "Program exited with code $EXIT_CODE"
    fi
    echo "  Output so far:"
    sed 's/^/    /' "$TMPDIR/output.txt"
fi

# ============================================================
# Test 3: All three threads produce output
# ============================================================
echo ""
echo "=== Test 3: All threads produce output ==="

if grep -q '\[Alpha\]' "$TMPDIR/output.txt"; then
    pass "Thread Alpha produced output"
else
    fail "Thread Alpha produced no output"
fi

if grep -q '\[Beta\]' "$TMPDIR/output.txt"; then
    pass "Thread Beta produced output"
else
    fail "Thread Beta produced no output"
fi

if grep -q '\[Gamma\]' "$TMPDIR/output.txt"; then
    pass "Thread Gamma produced output"
else
    fail "Thread Gamma produced no output"
fi

# ============================================================
# Test 4: All threads complete
# ============================================================
echo ""
echo "=== Test 4: All threads complete ==="

if grep -q '\[Alpha\] done' "$TMPDIR/output.txt"; then
    pass "Thread Alpha completed"
else
    fail "Thread Alpha did not complete"
fi

if grep -q '\[Beta\] done' "$TMPDIR/output.txt"; then
    pass "Thread Beta completed"
else
    fail "Thread Beta did not complete"
fi

if grep -q '\[Gamma\] done' "$TMPDIR/output.txt"; then
    pass "Thread Gamma completed"
else
    fail "Thread Gamma did not complete"
fi

if grep -q 'All threads finished' "$TMPDIR/output.txt"; then
    pass "Main thread reports all threads finished"
else
    fail "Main thread did not report all threads finished"
fi

# ============================================================
# Test 5: Interleaved execution
# ============================================================
echo ""
echo "=== Test 5: Interleaved execution ==="

# Extract thread labels in order of output lines
# We expect Alpha, Beta, Gamma to alternate, not run sequentially
LABELS=$(grep -oE '\[(Alpha|Beta|Gamma)\]' "$TMPDIR/output.txt" | head -9)

# Check that the first 3 labels are from 3 different threads
FIRST_THREE=$(echo "$LABELS" | head -3 | sort -u | wc -l | tr -d ' ')
if [ "$FIRST_THREE" -ge 3 ]; then
    pass "First 3 output lines are from 3 different threads (interleaved)"
else
    fail "First 3 output lines are NOT from different threads (may not be interleaved)"
    echo "  First 9 labels: $(echo $LABELS | tr '\n' ' ')"
fi

# Verify that threads don't run sequentially (all Alpha, then all Beta, etc.)
# Count the number of "transitions" between different thread labels
TRANSITIONS=0
PREV=""
while IFS= read -r label; do
    if [ -n "$PREV" ] && [ "$label" != "$PREV" ]; then
        TRANSITIONS=$((TRANSITIONS + 1))
    fi
    PREV="$label"
done <<< "$LABELS"

if [ "$TRANSITIONS" -ge 2 ]; then
    pass "Execution is interleaved ($TRANSITIONS context switches in first 9 lines)"
else
    fail "Execution does not appear interleaved ($TRANSITIONS transitions)"
fi

# ============================================================
# Test 6: Thread exit messages
# ============================================================
echo ""
echo "=== Test 6: Thread exit ==="

EXIT_COUNT=$(grep -c '\[uthread\].*exited' "$TMPDIR/output.txt" || true)
if [ "$EXIT_COUNT" -ge 3 ]; then
    pass "All 3 threads reported exit ($EXIT_COUNT exit messages)"
else
    fail "Expected 3 exit messages, got $EXIT_COUNT"
fi

# ============================================================
# Summary
# ============================================================
echo ""
echo "==============================="
echo "Results: $PASS passed, $FAIL failed"
echo "==============================="

if [ "$FAIL" -gt 0 ]; then
    echo ""
    echo "Full program output:"
    sed 's/^/  /' "$TMPDIR/output.txt"
    exit 1
fi
