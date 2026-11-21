#!/usr/bin/env bash
set -uo pipefail

LOG=build.txt
PARALLEL_ARGS=()

while [ $# -gt 0 ]; do
    case "$1" in
        --parallel)
            if [ $# -lt 2 ]; then
                echo "[ERROR] --parallel needs a job count, for example: ./build.sh --parallel 4"
                exit 1
            fi
            PARALLEL_ARGS=(--parallel "$2")
            shift 2
            ;;
        *)
            echo "[ERROR] Unknown argument: $1"
            exit 1
            ;;
    esac
done

: > "$LOG"

echo "[1/3] Checking tools..."
for tool in cmake ninja; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "[ERROR] $tool not found in PATH."
        exit 1
    fi
done

# CXX wins when set; otherwise CMake picks the system compiler
if [ -n "${CXX:-}" ]; then
    if ! command -v "$CXX" >/dev/null 2>&1; then
        echo "[ERROR] CXX is set to $CXX, which was not found in PATH."
        exit 1
    fi
elif ! command -v c++ >/dev/null 2>&1 && ! command -v g++ >/dev/null 2>&1 && ! command -v clang++ >/dev/null 2>&1; then
    echo "[ERROR] No C++ compiler found in PATH (c++, g++ or clang++)."
    exit 1
fi

echo "[2/3] Configuring project with CMake..."
echo "      First run fetches leveldb, quickjs and libdatachannel - this can take a while."
if ! cmake -B build -G Ninja -DCMAKE_MAKE_PROGRAM=ninja 2>&1 | tee -a "$LOG"; then
    echo "[ERROR] CMake configuration failed. See $LOG for details."
    exit 1
fi

echo "[3/3] Building FalconServer..."
cmake --build build --config Release ${PARALLEL_ARGS[@]+"${PARALLEL_ARGS[@]}"} 2>&1 | tee -a "$LOG"
BUILD_RESULT=${PIPESTATUS[0]}

if [ "$BUILD_RESULT" -ne 0 ]; then
    echo "[ERROR] Build failed. Full output saved to $LOG."
    exit 1
fi

if [ ! -f build/Falcon.Server/FalconServer ]; then
    echo "[ERROR] Build reported success but FalconServer was not produced. See $LOG."
    exit 1
fi

echo
echo "Build succeeded: build/Falcon.Server/FalconServer"
echo "Full build log saved to $LOG"
