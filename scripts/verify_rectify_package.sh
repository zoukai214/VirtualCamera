#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
package_root="${repo_root}/build/rectify_virtual_camera"

cd "${repo_root}"

cmake -S . -B build
cmake --build build -j"$(nproc)"
rm -rf "${package_root}"
cmake --install build --prefix "${package_root}"

test -f "${package_root}/virtual_camera_tool"
test -f "${package_root}/config.json"
test -f "${package_root}/image_virtual.bash"
test -d "${package_root}/output/x86/lib"

cmp "${package_root}/config.json" "${repo_root}/configs/config_rt024_parallel_run.json"

if grep -q 'build/' "${package_root}/image_virtual.bash"; then
  echo "image_virtual.bash unexpectedly references build/ path" >&2
  exit 1
fi

grep -q '\./virtual_camera_tool \./config.json' "${package_root}/image_virtual.bash"

echo "rectify package verification passed"
