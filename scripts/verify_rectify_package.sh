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

tool_backup="${package_root}/virtual_camera_tool.real"
args_log="${package_root}/tool_args.txt"
mv "${package_root}/virtual_camera_tool" "${tool_backup}"
trap 'rm -f "${package_root}/virtual_camera_tool" "${args_log}"; mv "${tool_backup}" "${package_root}/virtual_camera_tool"' EXIT

cat > "${package_root}/virtual_camera_tool" <<EOF
#!/usr/bin/env bash
printf '%s\n' "\$@" > "${args_log}"
EOF
chmod +x "${package_root}/virtual_camera_tool"

bash "${package_root}/image_virtual.bash" \
  --dataset_root /tmp/dataset \
  --config_path ./config.json \
  --output_root /tmp/output \
  --debug \
  --golden_root /tmp/golden

grep -q -- '--dataset_root' "${args_log}"
grep -q -- '/tmp/dataset' "${args_log}"
grep -q -- '--config_path' "${args_log}"
grep -q -- './config.json' "${args_log}"
grep -q -- '--output_root' "${args_log}"
grep -q -- '/tmp/output' "${args_log}"
grep -q -- '--debug' "${args_log}"
grep -q -- '--golden_root' "${args_log}"
grep -q -- '/tmp/golden' "${args_log}"

echo "rectify package verification passed"
