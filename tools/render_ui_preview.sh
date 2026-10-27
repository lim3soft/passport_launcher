#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
cmake -S tests/ui-preview -B build/ui-preview-host -DCMAKE_BUILD_TYPE=Debug
cmake --build build/ui-preview-host -j 4
mkdir -p build/ui-preview
ASAN_OPTIONS=detect_leaks=0 build/ui-preview-host/preview build/ui-preview
python3 tools/compose_ui_preview.py build/ui-preview
