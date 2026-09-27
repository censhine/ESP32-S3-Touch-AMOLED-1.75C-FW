#!/usr/bin/env bash
set -e -o pipefail
project_dir="$(cd "$(dirname "$0")/.." && pwd)"

if ! command -v idf.py >/dev/null 2>&1; then
    export IDF_TOOLS_PATH="${IDF_TOOLS_PATH:-$HOME/esp/tools-v5.5.5}"
    if [ -d /opt/homebrew/opt/python@3.12/libexec/bin ]; then
        export PATH="/opt/homebrew/opt/python@3.12/libexec/bin:$PATH"
    fi
    idf_export="${IDF_PATH:-$HOME/esp/esp-idf-v5.5.5}/export.sh"
    if [ ! -f "$idf_export" ]; then
        echo "Activate ESP-IDF 5.5.5 first (source its export.sh)." >&2
        exit 1
    fi
    source "$idf_export"
fi

idf.py -C "$project_dir/firmware" build
