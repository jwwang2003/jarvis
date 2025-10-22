#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Always rebuild the frontend first so firmware picks up fresh assets.
bash "${PROJECT_ROOT}/build_web.sh"

# Delegate ESP32 build to dedicated script while forwarding CLI flags.
bash "${PROJECT_ROOT}/build_esp32.sh" "$@"
