#!/usr/bin/env bash
set -euo pipefail
APP="/Applications/Azahar.app"
DEST="${HOME}/Library/Application Support/Azahar/sdmc/3ds/emerald3ds"
THREE_DSX="${DEST}/Emerald3DS.3dsx"
mkdir -p "$DEST"; : > "${DEST}/debug.txt"
open -a "$APP" --args "$THREE_DSX"
