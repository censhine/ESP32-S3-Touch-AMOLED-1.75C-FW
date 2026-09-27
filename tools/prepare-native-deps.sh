#!/usr/bin/env bash
set -e -o pipefail
project_dir="$(cd "$(dirname "$0")/.." && pwd)"
lvgl_source="$project_dir/firmware/managed_components/lvgl__lvgl"
if [ ! -f "$lvgl_source/lvgl.h" ]; then
    echo "Build the firmware first to resolve its pinned LVGL 9.4.0 dependency." >&2
    exit 1
fi
mkdir -p "$project_dir/vendor/lvgl"
cp -R "$lvgl_source/" "$project_dir/vendor/lvgl/"
