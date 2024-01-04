#!/bin/bash

function run_test() {
    for nthr in 32; do
        ./$1 $nthr > /dev/null
        for i in `seq 1 3`; do
            ./$1 $nthr
        done
        echo
    done
}

echo "test spin lock using xchg"
run_test "test-spinlock-xchg"

echo "test spin lock using cmpxchg"
run_test "test-spinlock-cmpxchg"

echo "test spin lock using pthread"
run_test "test-spinlock-pthread"

echo "test spin lock using Backoff"
run_test "test-spinlock-BackOff"

echo "test spin lock using MCS"
run_test "test-spinlock-MCS"

echo "test spin lock using CLH"
run_test "test-spinlock-CLH"
