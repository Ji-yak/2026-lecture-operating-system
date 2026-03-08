#!/bin/bash
#
# test_performance.sh - 성능 측정 테스트
#
# 1, 2, 4 threads로 coarse-grained과 fine-grained의 throughput을 비교합니다.
#
# 사용법: ./test_performance.sh [path_to_binary]
#

BINARY="${1:-./hashtable_bench}"
KEYS=200000
RUNS=3  # Average over multiple runs

if [ ! -x "$BINARY" ]; then
    echo "ERROR: '$BINARY' not found or not executable."
    echo "Usage: $0 [path_to_hashtable_bench]"
    exit 1
fi

echo "=== Hash Table Performance Test ==="
echo "Binary: $BINARY"
echo "Keys: $KEYS"
echo "Runs per config: $RUNS (averaged)"
echo ""

# Extract total time from output
extract_time() {
    echo "$1" | grep "Total time:" | awk '{print $3}'
}

# Extract insert ops/sec from output
extract_insert_ops() {
    echo "$1" | grep "Insert:" | sed 's/.*(\(.*\) ops.*/\1/'
}

# Extract lookup ops/sec from output
extract_lookup_ops() {
    echo "$1" | grep "Lookup:" | sed 's/.*(\(.*\) ops.*/\1/'
}

printf "%-12s %8s %15s %15s %12s\n" "Strategy" "Threads" "Insert ops/s" "Lookup ops/s" "Total (sec)"
printf "%-12s %8s %15s %15s %12s\n" "--------" "-------" "------------" "------------" "----------"

for strategy in coarse fine; do
    for threads in 1 2 4; do
        total_time_sum=0
        insert_ops_sum=0
        lookup_ops_sum=0

        for run in $(seq 1 $RUNS); do
            output=$("$BINARY" "$strategy" "$threads" "$KEYS" 2>&1)

            t=$(extract_time "$output")
            i=$(extract_insert_ops "$output")
            l=$(extract_lookup_ops "$output")

            # Use awk for floating-point arithmetic
            total_time_sum=$(echo "$total_time_sum $t" | awk '{printf "%.3f", $1 + $2}')
            insert_ops_sum=$(echo "$insert_ops_sum $i" | awk '{printf "%.0f", $1 + $2}')
            lookup_ops_sum=$(echo "$lookup_ops_sum $l" | awk '{printf "%.0f", $1 + $2}')
        done

        avg_time=$(echo "$total_time_sum $RUNS" | awk '{printf "%.3f", $1 / $2}')
        avg_insert=$(echo "$insert_ops_sum $RUNS" | awk '{printf "%.0f", $1 / $2}')
        avg_lookup=$(echo "$lookup_ops_sum $RUNS" | awk '{printf "%.0f", $1 / $2}')

        printf "%-12s %8d %15s %15s %12s\n" "$strategy" "$threads" "$avg_insert" "$avg_lookup" "$avg_time"
    done
    echo ""
done

echo "=== 분석 가이드 ==="
echo "1. thread 수가 증가할 때 coarse-grained의 성능 변화를 확인하세요."
echo "2. fine-grained이 coarse-grained보다 성능이 좋은 경우를 확인하세요."
echo "3. thread 수 증가에 따른 fine-grained의 확장성(scalability)을 분석하세요."
echo "4. 이상적인 경우 thread 수에 비례하여 throughput이 증가해야 합니다."
echo "   실제로는 lock contention, cache effects 등으로 선형 확장이 어렵습니다."
