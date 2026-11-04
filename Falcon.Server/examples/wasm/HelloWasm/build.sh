#!/usr/bin/env bash
# Builds HelloWasm.wasm. Needs wit-bindgen, a clang with the wasm32 target and a WASI sysroot (wasi-sdk has
# both): CLANG and WASI_SYSROOT point at them.
set -euo pipefail

cd "$(dirname "$0")"
: "${CLANG:=clang}"
: "${WASI_SYSROOT:?set WASI_SYSROOT to a WASI sysroot, such as wasi-sdk/share/wasi-sysroot}"
WIT=../../../wit

mkdir -p build
wit-bindgen c "$WIT" --out-dir build

# The server instantiates the core module wit-bindgen targets, not a component: no `wasm-tools component new`.
"$CLANG" --target=wasm32-wasip1 --sysroot="$WASI_SYSROOT" -mexec-model=reactor -O2 -Ibuild -I"$WIT/c" \
    -o build/HelloWasm.wasm HelloWasm.c "$WIT/c/falcon_wasm.c" build/falcon_plugin.c \
    build/falcon_plugin_component_type.o

echo "Built build/HelloWasm.wasm"
