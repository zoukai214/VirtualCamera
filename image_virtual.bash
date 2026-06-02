#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
tool_path="${script_dir}/virtual_camera_tool"
fallback_dir_name="build"

if [[ ! -x "${tool_path}" ]]; then
  tool_path="${script_dir}/${fallback_dir_name}/virtual_camera_tool"
fi

dataset_root=""
config_path=""
output_root=""
debug=false
golden_root=""

usage() {
  echo "Usage: $0 --dataset_root <path> --config_path <path> [--output_root <path>] [--debug --golden_root <path>]" >&2
}

require_value() {
  local flag="$1"
  local value="${2:-}"
  if [[ -z "${value}" || "${value}" == --* ]]; then
    echo "missing value for ${flag}" >&2
    usage
    exit 1
  fi
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --dataset_root)
      require_value "$1" "${2:-}"
      dataset_root="$2"
      shift 2
      ;;
    --config_path)
      require_value "$1" "${2:-}"
      config_path="$2"
      shift 2
      ;;
    --output_root)
      require_value "$1" "${2:-}"
      output_root="$2"
      shift 2
      ;;
    --debug)
      debug=true
      shift
      ;;
    --golden_root)
      require_value "$1" "${2:-}"
      golden_root="$2"
      shift 2
      ;;
    *)
      echo "unknown argument: $1" >&2
      usage
      exit 1
      ;;
  esac
done

if [[ -z "${dataset_root}" ]]; then
  echo "missing required flag --dataset_root" >&2
  usage
  exit 1
fi

if [[ -z "${config_path}" ]]; then
  echo "missing required flag --config_path" >&2
  usage
  exit 1
fi

if [[ -z "${output_root}" ]]; then
  output_root="${dataset_root}"
fi

if [[ "${debug}" == true && -z "${golden_root}" ]]; then
  echo "--debug requires --golden_root" >&2
  usage
  exit 1
fi

if [[ "${debug}" == false && -n "${golden_root}" ]]; then
  echo "--golden_root requires --debug" >&2
  usage
  exit 1
fi

cmd=(
  "${tool_path}"
  --dataset_root "${dataset_root}"
  --config_path "${config_path}"
  --output_root "${output_root}"
)

if [[ "${debug}" == true ]]; then
  cmd+=(--debug --golden_root "${golden_root}")
fi

"${cmd[@]}"
