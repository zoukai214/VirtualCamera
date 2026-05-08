#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

input_root="/workspace/L022/cfg/7v"
config_path="/workspace/VirtualCamera/configs/config_thor.json"
output_root="/workspace/VirtualCamera/output_verify/7v"

bash scripts/build.sh
rm -rf "$output_root"
./build/virtual_camera_tool generate-verify "$input_root" "$config_path" "$output_root"
