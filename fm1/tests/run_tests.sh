#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Drummatic FM-1: the host tests.
#   1. dm_compare: the port's engines, voices, sequencer and effects against Drummatic 0.2.3's own C++ DSP
#      (../Source/DSP), engine by engine and the whole machine (level within 1.5 dB);
#   2. the browser build driven like a hand on the panel (web/emu_test.mjs), when build/web exists and node is here;
#   3. with valgrind: the cost per sample of a few scenarios (tests/cpu_measure.sh), for information.
#   fm1/tests/run_tests.sh            (from the repository root or fm1/)
set -e
cd "$(dirname "$0")/.."
mkdir -p build/host
CC=${CC:-cc}
CXX=${CXX:-c++}
for f in dm_math dm_engines dm_voice dm_fx dm_kit dm_machine; do
    $CC -O2 -std=gnu99 -Wall -Wextra -Wno-unused-parameter -DDM_POOL= -c firmware/src/dm/$f.c -o build/host/$f.o
done
$CXX -O2 -std=c++17 -I../Source tests/dm_compare.cpp build/host/dm_*.o -o build/host/dm_compare
echo "== the port against Drummatic 0.2.3's DSP"
./build/host/dm_compare build/host > build/host/compare.txt || { cat build/host/compare.txt; exit 1; }
tail -4 build/host/compare.txt
echo "== the reference kit (\"Dael\" intro) against the owner's numbers"
$CC -O2 -std=gnu99 -DDM_POOL= tests/dm_ref.c firmware/src/dm/*.c -lm -o build/host/dm_ref
./build/host/dm_ref
echo "== the external MIDI clock"
$CC -O2 -std=gnu99 -DDM_POOL= tests/dm_clock.c firmware/src/dm/*.c -lm -o build/host/dm_clock
./build/host/dm_clock
if [ -f build/web/drummatic.wasm ] && command -v node >/dev/null 2>&1; then
    echo "== the panel in the browser build"
    node web/emu_test.mjs build/web/drummatic.wasm
fi
if command -v valgrind >/dev/null 2>&1 && [ "${CPU:-0}" = 1 ]; then
    echo "== cost (host instructions per sample; the FM-1: about 1.7 % of its time per 100, an estimate)"
    $CC -O2 -fno-tree-vectorize -std=gnu99 -DDM_POOL= tests/dm_cpu.c firmware/src/dm/*.c -o build/host/dm_cpu
    sh tests/cpu_measure.sh build/host/dm_cpu
fi
