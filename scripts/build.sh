#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."
rm -rf build
cmake -S . -B build
cmake --build build -j"$(nproc)"

