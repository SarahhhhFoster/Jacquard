#!/usr/bin/env bash
set -euo pipefail

BUILD_TYPE="${1:-Release}"
BUILD_DIR="build/Jacquard_artefacts/${BUILD_TYPE}"

VST3_DEST="${HOME}/Library/Audio/Plug-Ins/VST3"
AU_DEST="${HOME}/Library/Audio/Plug-Ins/Components"
CLAP_DEST="${HOME}/Library/Audio/Plug-Ins/CLAP"

install_plugin() {
    local src="$1"
    local dest_dir="$2"
    local name
    name="$(basename "${src}")"

    if [[ ! -e "${src}" ]]; then
        echo "  ⚠  Not found, skipping: ${src}"
        return
    fi

    mkdir -p "${dest_dir}"
    # Remove old copy first (rsync -delete doesn't work well with bundles)
    rm -rf "${dest_dir}/${name}"
    cp -R "${src}" "${dest_dir}/${name}"
    echo "  ✓  ${name}  →  ${dest_dir}"
}

echo "▶  Installing Jacquard plugins…"
install_plugin "${BUILD_DIR}/VST3/Jacquard.vst3"           "${VST3_DEST}"
install_plugin "${BUILD_DIR}/AU/Jacquard.component"         "${AU_DEST}"
install_plugin "${BUILD_DIR}/CLAP/Jacquard.clap"            "${CLAP_DEST}"

# Invalidate AU cache so the host re-scans
if command -v pluginkit &>/dev/null; then
    pluginkit -e use -i com.jacquard-audio.jacquard 2>/dev/null || true
fi
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo ""
echo "✓  Installation complete."
echo "   Restart your DAW to pick up the updated plugins."
