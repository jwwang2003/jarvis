#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
DEFAULT_PORT="/dev/ttyCH343USB0"
PORT="${PORT:-$DEFAULT_PORT}"
RUN_FLASH=true
RUN_MONITOR=true
IDF_SETUP_CMD=${IDF_SETUP_CMD:-'. "$HOME/esp/esp-idf/export.sh"'}

load_bash_startup() {
  if [[ -n "${BASH_STARTUP_LOADED:-}" ]]; then
    return
  fi

  shopt -s expand_aliases >/dev/null 2>&1 || true

  if [[ -f "${HOME}/.bashrc" ]]; then
    # shellcheck disable=SC1090
    source "${HOME}/.bashrc"
  fi

  BASH_STARTUP_LOADED=1
}

usage() {
  cat <<USAGE
Usage: $(basename "$0") [options]

Options:
  --port <device>     Serial port to use for flashing/monitor (default: ${PORT})
  --no-flash          Skip flashing the device
  --no-monitor        Skip monitoring after flash
  -h, --help          Show this help message
USAGE
}

while (($#)); do
  case "$1" in
    --port)
      if (($# < 2)); then
        echo "Error: --port requires an argument." >&2
        exit 1
      fi
      PORT="$2"
      shift
      ;;
    --port=*)
      PORT="${1#*=}"
      ;;
    --no-flash)
      RUN_FLASH=false
      ;;
    --no-monitor)
      RUN_MONITOR=false
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Error: Unknown option '$1'." >&2
      usage
      exit 1
      ;;
  esac
  shift
done

cd "${PROJECT_ROOT}"

prepare_idf_environment() {
  if command -v idf.py >/dev/null 2>&1; then
    return 0
  fi

  echo "idf.py not found, attempting to run: ${IDF_SETUP_CMD}"

  load_bash_startup

  if [[ "${IDF_SETUP_CMD}" == '. "$HOME/esp/esp-idf/export.sh"' ]]; then
    local export_script="${HOME}/esp/esp-idf/export.sh"
    if [[ ! -f "${export_script}" ]]; then
      echo "Error: Expected ESP-IDF export script at ${export_script} but it does not exist." >&2
      exit 1
    fi
  fi

  # shellcheck disable=SC2086
  eval "${IDF_SETUP_CMD}"

  if ! command -v idf.py >/dev/null 2>&1; then
    cat >&2 <<EOF
Error: '${IDF_SETUP_CMD}' executed, but idf.py is still unavailable.
       Verify that the setup command correctly exports the ESP-IDF environment.
EOF
    exit 1
  fi
}

idf_exec() {
  idf.py "$@"
}

prepare_idf_environment

if [[ ! -d "${BUILD_DIR}" ]]; then
  idf_exec set-target esp32s3
else
  rm -rf "${BUILD_DIR}"
fi

idf_exec build

if $RUN_FLASH; then
  idf_exec -p "${PORT}" flash
fi

if $RUN_MONITOR; then
  idf_exec -p "${PORT}" monitor
fi
