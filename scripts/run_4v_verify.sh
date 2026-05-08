#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
INPUT_ROOT="/workspace/L022/cfg/4v"
GOLDEN_MAPS="${INPUT_ROOT}/maps"
OUTPUT_ROOT="${ROOT_DIR}/output_verify/4v"
CONFIG_PATH="${ROOT_DIR}/configs/config_4v.yaml"
MAP_OUTPUT="${OUTPUT_ROOT}/maps"

rm -rf "${OUTPUT_ROOT}"
mkdir -p "${OUTPUT_ROOT}"

"${ROOT_DIR}/build/virtual_camera_tool" generate-4v "${CONFIG_PATH}"

for file in \
  front_map_x.bin front_map_y.bin front_mask.bin \
  rear_map_x.bin rear_map_y.bin rear_mask.bin \
  left_map_x.bin left_map_y.bin left_mask.bin \
  right_map_x.bin right_map_y.bin right_mask.bin \
  metadata.txt; do
  cmp "${GOLDEN_MAPS}/${file}" "${MAP_OUTPUT}/${file}"
done

python3 - <<PY
import math
import struct
from pathlib import Path

root = Path("${MAP_OUTPUT}")
width = 640
height = 640
weights = {}
for name in ("front", "rear", "left", "right"):
    data = (root / f"{name}_weight.bin").read_bytes()
    if len(data) != width * height * 4:
        raise SystemExit(f"{name}_weight.bin has invalid size")
    values = struct.unpack("<" + "f" * (width * height), data)
    for value in values:
        if not math.isfinite(value) or value < -1e-6 or value > 1.000001:
            raise SystemExit(f"{name}_weight.bin has invalid weight {value}")
    weights[name] = values

for index in range(width * height):
    total = sum(weights[name][index] for name in weights)
    if total > 1.000001:
        raise SystemExit(f"weight sum exceeds 1 at pixel {index}: {total}")
PY

echo "4v verification passed"
