# Merge GDC Add Cylinder Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the 4V YAML-driven `gdc_add_cylinder` map generator to `VirtualCamera` while keeping the existing 7V Thor generation and verification intact.

**Architecture:** Keep command paths separate: `generate-verify` remains the 7V Thor path, and a new `generate-4v <config.yaml>` command runs the 4V YAML path. Put 4V support in focused `virtual_camera` modules that wrap the original config loading, camera parameter loading, BEV map generation, bin writing, and cylinder map generation.

**Tech Stack:** C++17, CMake, OpenCV, Eigen, nlohmann_json, yaml-cpp, shell scripts for end-to-end verification.

---

## File Structure

- Create `include/virtual_camera/four_view_types.h`
  - Owns 4V-only structs copied and renamed from `gdc_add_cylinder` data types.
- Create `include/virtual_camera/four_view_config_loader.h`
  - Declares YAML config loading and validation.
- Create `src/four_view_config_loader.cpp`
  - Implements YAML parsing and relative path resolution.
- Create `include/virtual_camera/four_view_camera_params_loader.h`
  - Declares loading of `fisheye_cam_param.json`.
- Create `src/four_view_camera_params_loader.cpp`
  - Implements camera settings parsing with nlohmann_json.
- Create `include/virtual_camera/four_view_map_generator.h`
  - Declares BEV map, mask, and weight generation.
- Create `src/four_view_map_generator.cpp`
  - Ports `camera_maps_generator`, `weight_calculator`, and needed data helpers.
- Create `include/virtual_camera/four_view_bin_io.h`
  - Declares 4V bin output and golden comparison helpers.
- Create `src/four_view_bin_io.cpp`
  - Writes original-compatible `*_map_x.bin`, `*_map_y.bin`, `*_mask.bin`, `*_weight.bin`, and `metadata.txt`.
- Create `include/virtual_camera/cylinder_map_generator.h`
  - Declares cylinder map generation and save helpers.
- Create `src/cylinder_map_generator.cpp`
  - Ports `cylinder_map.cpp` into the `vc` namespace.
- Create `include/virtual_camera/four_view_runner.h`
  - Declares `int RunFourViewGenerate(const std::string& config_path)`.
- Create `src/four_view_runner.cpp`
  - Orchestrates the 4V flow.
- Create `tests/test_four_view_config.cpp`
  - Unit-tests YAML path resolution and config defaults.
- Create `tests/test_four_view_camera_params.cpp`
  - Unit-tests JSON camera order and required fields.
- Create `scripts/run_4v_verify.sh`
  - Builds a temporary YAML config and compares generated 4V output with `/workspace/L022/cfg/4v/maps`.
- Modify `CMakeLists.txt`
  - Add yaml-cpp discovery/linking, 4V sources, and new tests without changing compile options.
- Modify `src/main.cpp`
  - Add the `generate-4v` command branch.
- Modify `README.md`
  - Document 4V command and verification.

---

### Task 1: Add 4V Types And YAML Config Loader

**Files:**
- Create: `include/virtual_camera/four_view_types.h`
- Create: `include/virtual_camera/four_view_config_loader.h`
- Create: `src/four_view_config_loader.cpp`
- Create: `tests/test_four_view_config.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing config loader test**

Create `tests/test_four_view_config.cpp` with:

```cpp
#include "virtual_camera/four_view_config_loader.h"
#include "virtual_camera/json_utils.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace {

void WriteText(const std::filesystem::path& path, const std::string& text) {
  vc::EnsureDirectory(path.parent_path().string());
  std::ofstream output(path);
  output << text;
}

bool TestRelativePathsResolveFromConfigDir() {
  const std::filesystem::path root = "build/test_tmp/four_view_config";
  std::filesystem::remove_all(root);
  WriteText(root / "cfg/config.yaml",
            "input:\n"
            "  camera_params_file: fisheye_cam_param.json\n"
            "  image_params:\n"
            "    count: 4\n"
            "    image_width: 1280\n"
            "    image_height: 800\n"
            "output:\n"
            "  camera_maps_file: maps\n"
            "  stitched_result: stitched.jpg\n"
            "  output_format: bin\n"
            "stitching:\n"
            "  visual_world:\n"
            "    width: 12.0\n"
            "    height: 12.0\n"
            "  fusion:\n"
            "    parallel_range: 20.0\n"
            "    curve_range: 20.0\n"
            "    angles:\n"
            "      front_left: 75.0\n"
            "      front_right: 75.0\n"
            "      rear_left: 75.0\n"
            "      rear_right: 75.0\n"
            "image:\n"
            "  output_size:\n"
            "    width: 640\n"
            "    height: 640\n"
            "  vehicle:\n"
            "    width: 1950\n"
            "    length: 4855\n"
            "    overhang: 1068\n"
            "cylinder:\n"
            "  enabled: true\n"
            "  width: 768\n"
            "  height: 512\n"
            "  fx: 229.2\n"
            "  fy: 229.2\n"
            "  cx: 384.0\n"
            "  cy: 224.0\n"
            "  radius: 10000.0\n"
            "  output_dir: gdc\n");

  const auto config = vc::LoadFourViewConfig((root / "cfg/config.yaml").string());
  if (config.input.camera_params_file != (root / "cfg/fisheye_cam_param.json").string()) {
    std::cerr << "camera_params_file did not resolve relative to config dir\n";
    return false;
  }
  if (config.output.camera_maps_file != (root / "cfg/maps").string()) {
    std::cerr << "camera_maps_file did not resolve relative to config dir\n";
    return false;
  }
  if (!config.cylinder.enabled || config.cylinder.output_dir != (root / "cfg/gdc").string()) {
    std::cerr << "cylinder config was not loaded correctly\n";
    return false;
  }
  return true;
}

bool TestMissingConfigThrows() {
  try {
    (void)vc::LoadFourViewConfig("build/test_tmp/no_such_4v_config.yaml");
  } catch (const std::runtime_error& ex) {
    return std::string(ex.what()).find("failed to load 4v yaml") != std::string::npos;
  }
  std::cerr << "expected missing config exception\n";
  return false;
}

}  // namespace

int main() {
  if (!TestRelativePathsResolveFromConfigDir()) {
    return 1;
  }
  if (!TestMissingConfigThrows()) {
    return 1;
  }
  return 0;
}
```

- [ ] **Step 2: Add the test target and run it to verify it fails**

Modify `CMakeLists.txt` to add a temporary test target after existing tests:

```cmake
if(EXISTS ${PROJECT_SOURCE_DIR}/tests/test_four_view_config.cpp)
    add_executable(test_four_view_config tests/test_four_view_config.cpp)
    target_link_libraries(test_four_view_config PRIVATE virtual_camera_core)
endif()
```

Run:

```bash
bash scripts/build.sh
```

Expected: build fails because `virtual_camera/four_view_config_loader.h` does not exist.

- [ ] **Step 3: Add 4V types**

Create `include/virtual_camera/four_view_types.h` with structs for:

```cpp
namespace vc {

struct FourViewImageParams {
  int count = 4;
  int image_width = 1280;
  int image_height = 800;
};

struct FourViewInputConfig {
  std::string camera_params_file;
  FourViewImageParams image_params;
};

struct FourViewOutputConfig {
  std::string camera_maps_file;
  std::string stitched_result = "stitched_result_optimized.jpg";
  std::string output_format = "bin";
};

struct FourViewFusionAngles {
  double front_left = 75.0;
  double front_right = 75.0;
  double rear_left = 75.0;
  double rear_right = 75.0;
};

struct FourViewStitchingConfig {
  double visual_world_width = 12.0;
  double visual_world_height = 12.0;
  double parallel_range = 20.0;
  double curve_range = 20.0;
  FourViewFusionAngles angles;
};

struct FourViewImageConfig {
  int output_width = 640;
  int output_height = 640;
  double vehicle_width_mm = 1950.0;
  double vehicle_length_mm = 4855.0;
  double vehicle_overhang_mm = 1068.0;
};

struct FourViewCylinderConfig {
  bool enabled = false;
  int width = 768;
  int height = 512;
  double fx = 229.18;
  double fy = 229.18;
  double cx = 384.0;
  double cy = 224.0;
  double radius = 10000.0;
  std::string output_dir;
};

struct FourViewConfig {
  FourViewInputConfig input;
  FourViewOutputConfig output;
  FourViewStitchingConfig stitching;
  FourViewImageConfig image;
  FourViewCylinderConfig cylinder;
};

}  // namespace vc
```

- [ ] **Step 4: Implement the config loader**

Create `include/virtual_camera/four_view_config_loader.h`:

```cpp
#pragma once

#include "virtual_camera/four_view_types.h"

#include <string>

namespace vc {

FourViewConfig LoadFourViewConfig(const std::string& config_path);

}  // namespace vc
```

Create `src/four_view_config_loader.cpp` using `yaml-cpp/yaml.h`, `std::filesystem`, and helpers:

```cpp
namespace {

std::string ResolvePath(const std::filesystem::path& config_dir,
                        const std::string& raw_path) {
  if (raw_path.empty()) {
    return raw_path;
  }
  const std::filesystem::path path(raw_path);
  if (path.is_absolute()) {
    return path.string();
  }
  return (config_dir / path).lexically_normal().string();
}

template <typename T>
T ReadValue(const YAML::Node& node, const std::string& key, const T& fallback) {
  if (!node || !node[key]) {
    return fallback;
  }
  return node[key].as<T>();
}

}  // namespace
```

Populate every `FourViewConfig` field from the YAML tree and throw `std::runtime_error("failed to load 4v yaml: " + config_path)` when `YAML::LoadFile` fails.

- [ ] **Step 5: Link yaml-cpp and run the config test**

Modify `CMakeLists.txt`:

```cmake
find_package(yaml-cpp REQUIRED)
```

Add `src/four_view_config_loader.cpp` to `VIRTUAL_CAMERA_CORE_SOURCES`.

Add `yaml-cpp` to `target_link_libraries(virtual_camera_core PUBLIC ...)`.

Run:

```bash
bash scripts/build.sh
./build/test_four_view_config
```

Expected: both commands exit 0.

- [ ] **Step 6: Commit**

```bash
git add CMakeLists.txt include/virtual_camera/four_view_types.h include/virtual_camera/four_view_config_loader.h src/four_view_config_loader.cpp tests/test_four_view_config.cpp
git commit -m "feat: add four view yaml config loader"
```

---

### Task 2: Add 4V Camera Parameter Loading

**Files:**
- Create: `include/virtual_camera/four_view_camera_params_loader.h`
- Create: `src/four_view_camera_params_loader.cpp`
- Create: `tests/test_four_view_camera_params.cpp`
- Modify: `include/virtual_camera/four_view_types.h`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing camera parameter test**

Create `tests/test_four_view_camera_params.cpp` with a minimal `camera_settings` JSON containing `front`, `left`, `right`, and `back`. Assert:

```cpp
const auto params = vc::LoadFourViewCameraParams(json_path, 4);
params.cameras.at(0).name == "left";
params.cameras.at(1).name == "front";
params.cameras.at(2).name == "right";
params.cameras.at(3).name == "rear";
params.cameras.at(1).intrinsic_src(0, 0) == 447.0;
params.cameras.at(3).distortion.size() == 4;
params.cameras.at(1).extrinsic(3, 3) == 1.0;
```

- [ ] **Step 2: Add the test target and verify it fails**

Add:

```cmake
if(EXISTS ${PROJECT_SOURCE_DIR}/tests/test_four_view_camera_params.cpp)
    add_executable(test_four_view_camera_params tests/test_four_view_camera_params.cpp)
    target_link_libraries(test_four_view_camera_params PRIVATE virtual_camera_core)
endif()
```

Run:

```bash
bash scripts/build.sh
```

Expected: build fails because the loader header is missing.

- [ ] **Step 3: Extend 4V types**

Add to `four_view_types.h`:

```cpp
struct FourViewCameraParam {
  std::string name;
  Eigen::Matrix3d intrinsic = Eigen::Matrix3d::Identity();
  Eigen::Matrix3d intrinsic_src = Eigen::Matrix3d::Identity();
  Eigen::Matrix4d extrinsic = Eigen::Matrix4d::Identity();
  std::vector<double> distortion;
};

struct FourViewCameraParams {
  std::vector<FourViewCameraParam> cameras;
};
```

Include `<Eigen/Dense>` and `<vector>`.

- [ ] **Step 4: Implement the camera parameter loader**

Create `include/virtual_camera/four_view_camera_params_loader.h`:

```cpp
#pragma once

#include "virtual_camera/four_view_types.h"

#include <string>

namespace vc {

FourViewCameraParams LoadFourViewCameraParams(const std::string& json_path,
                                              int camera_count);

}  // namespace vc
```

Implement `src/four_view_camera_params_loader.cpp` with nlohmann_json and this mapping:

```cpp
const std::vector<std::pair<std::string, int>> camera_order = {
    {"left", 0}, {"front", 1}, {"right", 2}, {"back", 3}, {"rear", 3}};
```

Parse `intrinsics`, `intrinsics_src`, `distort`, and `extrinsics.pose`; store `back` as name `rear`.

- [ ] **Step 5: Run the camera parameter tests**

Run:

```bash
bash scripts/build.sh
./build/test_four_view_camera_params
```

Expected: both commands exit 0.

- [ ] **Step 6: Commit**

```bash
git add CMakeLists.txt include/virtual_camera/four_view_types.h include/virtual_camera/four_view_camera_params_loader.h src/four_view_camera_params_loader.cpp tests/test_four_view_camera_params.cpp
git commit -m "feat: add four view camera params loader"
```

---

### Task 3: Port 4V BEV And Cylinder Generation

**Files:**
- Create: `include/virtual_camera/four_view_map_generator.h`
- Create: `src/four_view_map_generator.cpp`
- Create: `include/virtual_camera/four_view_bin_io.h`
- Create: `src/four_view_bin_io.cpp`
- Create: `include/virtual_camera/cylinder_map_generator.h`
- Create: `src/cylinder_map_generator.cpp`
- Modify: `include/virtual_camera/four_view_types.h`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add map output types**

Add to `four_view_types.h`:

```cpp
struct FourViewSingleCameraMaps {
  cv::Mat map_x;
  cv::Mat map_y;
  cv::Mat mask;
  cv::Mat weight;
};

struct FourViewCameraMaps {
  std::map<std::string, FourViewSingleCameraMaps> by_name;
  int width = 0;
  int height = 0;
};
```

Include `<map>` and `<opencv2/core.hpp>`.

- [ ] **Step 2: Create generator headers**

Create `four_view_map_generator.h`:

```cpp
#pragma once

#include "virtual_camera/four_view_types.h"

namespace vc {

FourViewCameraMaps GenerateFourViewMaps(const FourViewConfig& config,
                                        const FourViewCameraParams& camera_params);

}  // namespace vc
```

Create `four_view_bin_io.h`:

```cpp
#pragma once

#include "virtual_camera/four_view_types.h"

#include <string>

namespace vc {

void SaveFourViewMapsBin(const FourViewCameraMaps& maps,
                         const std::string& output_dir);

}  // namespace vc
```

Create `cylinder_map_generator.h`:

```cpp
#pragma once

#include "virtual_camera/four_view_types.h"

#include <string>

namespace vc {

void GenerateAndSaveCylinderMaps(const FourViewConfig& config,
                                 const FourViewCameraParams& camera_params);

}  // namespace vc
```

- [ ] **Step 3: Port BEV generation**

Copy the required logic from:

- `/workspace/gdc_add_cylinder/GStitchMapsGenerate/include/data_types.h`
- `/workspace/gdc_add_cylinder/GStitchMapsGenerate/src/data_types.cpp`
- `/workspace/gdc_add_cylinder/GStitchMapsGenerate/include/camera_maps_generator.h`
- `/workspace/gdc_add_cylinder/GStitchMapsGenerate/src/camera_maps_generator.cpp`
- `/workspace/gdc_add_cylinder/GStitchMapsGenerate/include/weight_calculator.h`
- `/workspace/gdc_add_cylinder/GStitchMapsGenerate/src/weight_calculator.cpp`

Port it into `src/four_view_map_generator.cpp` under `namespace vc`. Preserve formulas and camera naming, but use `FourViewConfig` and `FourViewCameraParams` as inputs instead of the original classes.

- [ ] **Step 4: Port bin writing**

Copy the compatible output behavior from:

- `/workspace/gdc_add_cylinder/GStitchMapsGenerate/include/bin_file_io.h`
- `/workspace/gdc_add_cylinder/GStitchMapsGenerate/src/bin_file_io.cpp`

Implement `SaveFourViewMapsBin` so it writes:

```text
front_map_x.bin
front_map_y.bin
front_mask.bin
front_weight.bin
rear_map_x.bin
rear_map_y.bin
rear_mask.bin
rear_weight.bin
left_map_x.bin
left_map_y.bin
left_mask.bin
left_weight.bin
right_map_x.bin
right_map_y.bin
right_mask.bin
right_weight.bin
metadata.txt
```

- [ ] **Step 5: Port cylinder generation**

Copy `/workspace/gdc_add_cylinder/GStitchMapsGenerate/src/cylinder_map.cpp` into `src/cylinder_map_generator.cpp`, convert to `namespace vc`, and wrap it in:

```cpp
void GenerateAndSaveCylinderMaps(const FourViewConfig& config,
                                 const FourViewCameraParams& camera_params);
```

Use the original mapping:

```cpp
struct CylinderCamInfo {
  int loader_idx;
  int cylinder_cam_id;
  std::string name;
};
const CylinderCamInfo cam_infos[] = {
    {1, 0, "front"},
    {0, 1, "left"},
    {2, 2, "right"},
    {3, 3, "rear"},
};
```

- [ ] **Step 6: Add sources to the core library and build**

Add these to `VIRTUAL_CAMERA_CORE_SOURCES`:

```cmake
src/four_view_map_generator.cpp
src/four_view_bin_io.cpp
src/cylinder_map_generator.cpp
```

Run:

```bash
bash scripts/build.sh
./build/test_four_view_config
./build/test_four_view_camera_params
```

Expected: all commands exit 0.

- [ ] **Step 7: Commit**

```bash
git add CMakeLists.txt include/virtual_camera/four_view_types.h include/virtual_camera/four_view_map_generator.h include/virtual_camera/four_view_bin_io.h include/virtual_camera/cylinder_map_generator.h src/four_view_map_generator.cpp src/four_view_bin_io.cpp src/cylinder_map_generator.cpp
git commit -m "feat: port four view map generators"
```

---

### Task 4: Add 4V Runner And CLI Command

**Files:**
- Create: `include/virtual_camera/four_view_runner.h`
- Create: `src/four_view_runner.cpp`
- Modify: `src/main.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add runner interface**

Create `include/virtual_camera/four_view_runner.h`:

```cpp
#pragma once

#include <string>

namespace vc {

int RunFourViewGenerate(const std::string& config_path);

}  // namespace vc
```

- [ ] **Step 2: Implement runner**

Create `src/four_view_runner.cpp`:

```cpp
#include "virtual_camera/four_view_runner.h"

#include "virtual_camera/cylinder_map_generator.h"
#include "virtual_camera/four_view_bin_io.h"
#include "virtual_camera/four_view_camera_params_loader.h"
#include "virtual_camera/four_view_config_loader.h"
#include "virtual_camera/four_view_map_generator.h"

#include <iostream>

namespace vc {

int RunFourViewGenerate(const std::string& config_path) {
  const auto config = LoadFourViewConfig(config_path);
  const auto camera_params =
      LoadFourViewCameraParams(config.input.camera_params_file,
                               config.input.image_params.count);
  const auto maps = GenerateFourViewMaps(config, camera_params);
  SaveFourViewMapsBin(maps, config.output.camera_maps_file);
  if (config.cylinder.enabled) {
    GenerateAndSaveCylinderMaps(config, camera_params);
  }
  std::cout << "4v generation passed\n";
  return 0;
}

}  // namespace vc
```

- [ ] **Step 3: Add CLI branch**

Modify `src/main.cpp`:

```cpp
#include "virtual_camera/four_view_runner.h"
```

Update `main` argument handling:

```cpp
if (argc == 3 && std::string(argv[1]) == "generate-4v") {
  try {
    return vc::RunFourViewGenerate(argv[2]);
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << "\n";
    return 1;
  }
}
```

Then keep the existing `generate-verify` path unchanged.

- [ ] **Step 4: Add runner source and build**

Add `src/four_view_runner.cpp` to `VIRTUAL_CAMERA_CORE_SOURCES`.

Run:

```bash
bash scripts/build.sh
./build/virtual_camera_tool generate-4v /workspace/gdc_add_cylinder/GStitchMapsGenerate/config/config.yaml
```

Expected: build passes; the command may write to the YAML absolute paths unless the test config is adjusted in Task 5.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt include/virtual_camera/four_view_runner.h src/four_view_runner.cpp src/main.cpp
git commit -m "feat: add four view generation command"
```

---

### Task 5: Add 4V Verification Script And README

**Files:**
- Create: `scripts/run_4v_verify.sh`
- Modify: `README.md`

- [ ] **Step 1: Write verification script**

Create `scripts/run_4v_verify.sh`:

```bash
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
  front_map_x.bin front_map_y.bin front_mask.bin front_weight.bin \
  rear_map_x.bin rear_map_y.bin rear_mask.bin rear_weight.bin \
  left_map_x.bin left_map_y.bin left_mask.bin left_weight.bin \
  right_map_x.bin right_map_y.bin right_mask.bin right_weight.bin \
  metadata.txt; do
  cmp "${GOLDEN_MAPS}/${file}" "${MAP_OUTPUT}/${file}"
done

echo "4v verification passed"
```

- [ ] **Step 2: Make it executable and run it**

Run:

```bash
chmod +x scripts/run_4v_verify.sh
bash scripts/build.sh
bash scripts/run_4v_verify.sh
```

Expected: prints `4v verification passed`.

- [ ] **Step 3: Document commands**

Update `README.md` with:

```markdown
## Generate And Verify 4V

```bash
bash scripts/build.sh
bash scripts/run_4v_verify.sh
```

The 4V command keeps the original YAML style from `gdc_add_cylinder`:

```bash
./build/virtual_camera_tool generate-4v <config.yaml>
```
```

- [ ] **Step 4: Run full regression**

Run:

```bash
bash scripts/build.sh
bash scripts/run_4v_verify.sh
bash scripts/run_thor_verify.sh
```

Expected:

```text
4v verification passed
verification passed
```

- [ ] **Step 5: Commit**

```bash
git add README.md scripts/run_4v_verify.sh
git commit -m "test: add four view verification"
```

---

## Self-Review Notes

- Spec coverage: Tasks cover the new CLI command, YAML compatibility, camera JSON loading, BEV output, cylinder output generation, CMake integration, README, 4V verification, and 7V regression.
- Placeholder scan: The plan intentionally names exact files, commands, and expected outputs. Implementation copy/port steps reference concrete source files in `/workspace/gdc_add_cylinder`.
- Type consistency: Public 4V functions consistently use `FourViewConfig`, `FourViewCameraParams`, and `FourViewCameraMaps`.
