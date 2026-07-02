#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
SIM_DIR="${ROOT_DIR}/sim"
WASM_DIR="${SIM_DIR}/wasm"
BUILD_DIR="${WASM_DIR}/build"
OUT_DIR="${SIM_DIR}/web/public/wasm"

if ! command -v emcmake >/dev/null 2>&1; then
    echo "Error: emcmake not found. Emscripten is required to build the WASM simulator."
    echo ""
    echo "Install/activate Emscripten, for example:"
    echo "  git clone https://github.com/emscripten-core/emsdk.git"
    echo "  cd emsdk"
    echo "  ./emsdk install latest"
    echo "  ./emsdk activate latest"
    echo "  source ./emsdk_env.sh"
    echo ""
    echo "Then rerun this script."
    exit 1
fi

if ! command -v emcc >/dev/null 2>&1; then
    echo "Error: emcc not found. Emscripten is required to build the WASM simulator."
    echo "See the instructions above for installing Emscripten."
    exit 1
fi

mkdir -p "${BUILD_DIR}"
mkdir -p "${OUT_DIR}"

rm -f "${OUT_DIR}"/*

emcmake cmake -S "${SIM_DIR}" -B "${BUILD_DIR}"
emmake cmake --build "${BUILD_DIR}" --target gyverlamp_sim_wasm -j4

echo ""
echo "WASM build succeeded."
echo "Output: ${OUT_DIR}"
ls -la "${OUT_DIR}"
