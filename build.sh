#!/usr/bin/env bash
set -euo pipefail

BUILD_TYPE="${1:-Release}"
BUILD_DIR="build"
JOBS="$(sysctl -n hw.logicalcpu)"

echo "▶  Configuring (${BUILD_TYPE})…"
cmake -B "${BUILD_DIR}" \
      -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
      -G "Unix Makefiles"

echo "▶  Building with ${JOBS} parallel jobs…"
cmake --build "${BUILD_DIR}" \
      --config "${BUILD_TYPE}" \
      -- -j"${JOBS}"

echo ""
echo "✓  Build complete.  Artefacts:"
find "${BUILD_DIR}/Jacquard_artefacts/${BUILD_TYPE}" \
     -maxdepth 1 \( -name "*.vst3" -o -name "*.component" -o -name "*.clap" -o -name "*.app" \) \
  | sort \
  | sed 's/^/   /'
