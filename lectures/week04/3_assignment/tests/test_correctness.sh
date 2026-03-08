#!/bin/bash
#
# test_correctness.sh - 정확성 테스트
#
# 모든 삽입된 키가 올바르게 조회되는지 확인합니다.
# coarse-grained과 fine-grained 모두 테스트합니다.
#
# 사용법: ./test_correctness.sh [path_to_binary]
#

set -e

BINARY="${1:-./hashtable_bench}"
PASS=0
FAIL=0
KEYS=50000

if [ ! -x "$BINARY" ]; then
    echo "ERROR: '$BINARY' not found or not executable."
    echo "Usage: $0 [path_to_hashtable_bench]"
    exit 1
fi

run_test() {
    local strategy=$1
    local threads=$2
    local desc="$strategy, $threads threads, $KEYS keys"

    echo -n "TEST: $desc ... "

    output=$("$BINARY" "$strategy" "$threads" "$KEYS" 2>&1)

    if echo "$output" | grep -q "All keys found correctly"; then
        echo "PASS"
        PASS=$((PASS + 1))
    elif echo "$output" | grep -q "WARNING.*keys were lost"; then
        missing=$(echo "$output" | grep "Missing:" | sed 's/.*Missing: //' | tr -d ' ')
        echo "FAIL (lost keys: $missing)"
        FAIL=$((FAIL + 1))
    else
        echo "FAIL (unexpected output)"
        echo "$output"
        FAIL=$((FAIL + 1))
    fi
}

echo "=== Hash Table Correctness Tests ==="
echo "Binary: $BINARY"
echo "Keys per test: $KEYS"
echo ""

# Single thread (baseline - should always pass)
run_test coarse 1
run_test fine   1

# Multiple threads - coarse-grained
run_test coarse 2
run_test coarse 4
run_test coarse 8

# Multiple threads - fine-grained
run_test fine   2
run_test fine   4
run_test fine   8

# Stress test with many threads
run_test coarse 16
run_test fine   16

echo ""
echo "=== Results ==="
echo "Passed: $PASS"
echo "Failed: $FAIL"

if [ "$FAIL" -eq 0 ]; then
    echo "All tests passed!"
    exit 0
else
    echo "Some tests failed."
    exit 1
fi
