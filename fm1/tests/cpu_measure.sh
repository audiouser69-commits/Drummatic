#!/bin/sh
# Drummatic FM-1: instructions per sample of each scenario of tests/dm_cpu.c in its steady state
# (halves 1000..1200: the sleeping FX asleep), counted by valgrind.  tests/cpu_measure.sh BUILD/dm_cpu
B=${1:-build/cpu/dm_cpu}
for sc in 4 0 1 2 3; do
    a=$(valgrind --tool=callgrind --callgrind-out-file=/dev/null "$B" $sc 1000 2>&1 | grep Collected | awk '{print $NF}')
    b=$(valgrind --tool=callgrind --callgrind-out-file=/dev/null "$B" $sc 1200 2>&1 | grep Collected | awk '{print $NF}')
    echo "scenario $sc: $(( (b - a) / (200 * 128) )) instr/sample"
done
