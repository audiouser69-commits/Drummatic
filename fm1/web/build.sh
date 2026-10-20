#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Drummatic FM-1 in the browser: the firmware's sources (web/dm_web.c) to WebAssembly with clang + wasm-ld
# (no Emscripten, no libc), plus the page and the worklet.  Run tools/build.py once first (build/gen: the fonts).
#   web/build.sh  ->  build/web/{index.html, worklet.js, drummatic.wasm, fonts/}
set -e
cd "$(dirname "$0")/.."
[ -f build/gen/ui_fonts.h ] || { echo "web: run tools/build.py once first (build/gen)"; exit 1; }
CLANG=${CLANG:-clang}
WASMLD=${WASMLD:-wasm-ld}
mkdir -p build/web/fonts
"$CLANG" --target=wasm32 -O2 -ffp-contract=off -std=gnu99 -nostdlib -fno-builtin -w \
    -Ibuild/gen -Ifirmware/src -Ifirmware/src/platform -Ifirmware/hal -c web/dm_web.c -o build/web/dm_web.o
"$WASMLD" --no-entry --export-dynamic -z stack-size=1048576 --initial-memory=33554432 \
    build/web/dm_web.o -o build/web/drummatic.wasm
VERSION=$(sed -n 's/^#define DM_VERSION "\([^"]*\)".*/\1/p' firmware/src/app/dm_core.h)
BUILD=$(git rev-parse --short HEAD 2>/dev/null || echo local)
sed -e "s/@VERSION@/$VERSION/g" -e "s/@BUILD@/$BUILD/g" web/index.html > build/web/index.html
cp web/worklet.js web/silence.wav web/favicon.png build/web/
cp web/fonts/* build/web/fonts/
echo "web: build/web ($(wc -c < build/web/drummatic.wasm | tr -d ' ') B wasm, $VERSION, $BUILD)"
