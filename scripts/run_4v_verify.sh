#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
INPUT_ROOT="/workspace/L022/cfg/4v"
GOLDEN_MAPS="${INPUT_ROOT}/maps"
OUTPUT_ROOT="${ROOT_DIR}/output_verify/4v"
CONFIG_PATH="${OUTPUT_ROOT}/config.yaml"
MAP_OUTPUT="${OUTPUT_ROOT}/maps"
CYLINDER_OUTPUT="${OUTPUT_ROOT}/gdc"

rm -rf "${OUTPUT_ROOT}"
mkdir -p "${OUTPUT_ROOT}"

cat > "${CONFIG_PATH}" <<YAML
input:
  camera_params_file: "${INPUT_ROOT}/fisheye_cam_param.json"
  image_params:
    count: 4
    image_width: 1280
    image_height: 800
output:
  camera_maps_file: "${MAP_OUTPUT}"
  stitched_result: "${OUTPUT_ROOT}/stitched_result_optimized.jpg"
  output_format: "bin"
stitching:
  visual_world:
    width: 12.0
    height: 12.0
  fusion:
    parallel_range: 20.0
    curve_range: 20.0
    angles:
      front_left: 75.0
      front_right: 75.0
      rear_left: 75.0
      rear_right: 75.0
image:
  output_size:
    width: 640
    height: 640
  vehicle:
    width: 1950
    length: 4855
    overhang: 1068
cylinder:
  enabled: true
  width: 768
  height: 512
  fx: 229.2
  fy: 229.2
  cx: 384.0
  cy: 224.0
  radius: 10000.0
  output_dir: "${CYLINDER_OUTPUT}"
YAML

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
