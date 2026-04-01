#!/usr/bin/env bash
set -euo pipefail

VST3_PATH="${HOME}/Library/Audio/Plug-Ins/VST3/Jacquard.vst3"
AU_PATH="${HOME}/Library/Audio/Plug-Ins/Components/Jacquard.component"
CLAP_PATH="${HOME}/Library/Audio/Plug-Ins/CLAP/Jacquard.clap"
APP_PATH="${HOME}/Applications/Jacquard.app"

remove_if_exists() {
    if [[ -e "$1" ]]; then
        rm -rf "$1"
        echo "  ✓  Removed: $1"
    else
        echo "  –  Not installed: $1"
    fi
}

echo "▶  Uninstalling Jacquard…"
remove_if_exists "${VST3_PATH}"
remove_if_exists "${AU_PATH}"
remove_if_exists "${CLAP_PATH}"
remove_if_exists "${APP_PATH}"

# Invalidate AU cache
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo ""
echo "✓  Uninstallation complete."
echo "   Restart your DAW to complete removal."
