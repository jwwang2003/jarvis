#!/usr/bin/env bash
set -euo pipefail

# Resolve repository root so the script works from any invocation directory.
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

ensure_command() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Error: Required command '$1' is not installed or not in PATH." >&2
    exit 1
  fi
}

ensure_command npm
ensure_command pnpm

cd "${PROJECT_ROOT}/web"

pnpm install
pnpm run build

npx svelteesp32 -e espidf -s ../web/dist -o ../main/includes/svelteesp32.h --etag=true
