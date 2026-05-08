# X86 Thor Bin Verification Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build an x86-only optimized virtual-camera generator in `/workspace/VirtualCamera` that reads `/workspace/L022/cfg/7v/`, generates Thor 7v outputs under `output_verify/7v/`, and verifies `gdc` plus `gdc_intri` bins byte-for-byte against the golden data.

**Architecture:** Create a focused C++17 project with a small domain model, JSON/path utilities, task expansion from `config_thor.json`, map generation, output writing, and a verifier. Keep generated outputs outside source control and never modify `/workspace/icv_vc_bin_lib` or `/workspace/L022/cfg/7v/`.

**Tech Stack:** C++17, CMake, OpenCV, Eigen, nlohmann/json, Bash, Git.

---

## File Structure

- Create `CMakeLists.txt`: x86-only build definition for the app and test executables.
- Create `README.md`: build/run/verify instructions and workspace constraints.
- Create `.gitignore`: exclude build products and generated verification output.
- Create `configs/config_thor.json`: copy of the Thor task configuration used as the runtime task source.
- Create `include/virtual_camera/types.h`: shared structs and camera-name mappings.
- Create `include/virtual_camera/json_utils.h`: JSON loading and matrix/vector conversion declarations.
- Create `src/json_utils.cpp`: JSON and filesystem helper implementation.
- Create `include/virtual_camera/task_builder.h`: task expansion interface.
- Create `src/task_builder.cpp`: expand `config_thor.json` into virtual, undistort, and resize tasks.
- Create `include/virtual_camera/map_generator.h`: virtual camera, undistort, resize, and bin writer interfaces.
- Create `src/map_generator.cpp`: OpenCV/Eigen map generation implementation.
- Create `include/virtual_camera/json_writer.h`: virtual JSON output interface.
- Create `src/json_writer.cpp`: deterministic field updates for virtual JSON files.
- Create `include/virtual_camera/verifier.h`: byte and JSON-field verifier interface.
- Create `src/verifier.cpp`: golden comparison implementation.
- Create `src/main.cpp`: CLI entry point for generate and verify flow.
- Create `tests/test_task_builder.cpp`: focused task expansion checks.
- Create `tests/test_verifier.cpp`: byte compare and JSON-field compare checks.
- Create `scripts/build.sh`: clean x86 build.
- Create `scripts/run_thor_verify.sh`: generate and verify `/workspace/L022/cfg/7v/`.

## Task 1: Repository Scaffold

**Files:**
- Create: `.gitignore`
- Create: `README.md`
- Create: `CMakeLists.txt`
- Create: `scripts/build.sh`
- Create: `scripts/run_thor_verify.sh`

- [ ] **Step 1: Write repository ignore rules**

Create `.gitignore`:

```gitignore
build/
output_verify/
compile_commands.json
*.o
*.a
*.so
*.bin
*.checkcode
```

- [ ] **Step 2: Write project README**

Create `README.md`:

```markdown
# VirtualCamera

X86-only virtual camera map generator and verifier for the L022 7v Thor configuration.

## Workspace Rules

- Source and git operations live in `/workspace/VirtualCamera`.
- `/workspace/icv_vc_bin_lib` is reference-only and must not be modified.
- `/workspace/L022/cfg/7v/` is input and golden data only; generated output is written to `output_verify/7v/`.

## Build

```bash
bash scripts/build.sh
```

## Generate And Verify

```bash
bash scripts/run_thor_verify.sh
```

The verifier requires:

- `gdc/*.bin` byte-for-byte equal to `/workspace/L022/cfg/7v/calib/gdc`.
- `gdc_intri/*.bin` byte-for-byte equal to `/workspace/L022/cfg/7v/calib/gdc_intri`.
- `virtual/**/*.json` key calibration fields equal to `/workspace/L022/cfg/7v/calib/virtual`.
```

- [ ] **Step 3: Write initial CMake file**

Create `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.10)
project(VirtualCamera LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Eigen3 REQUIRED)
find_package(OpenCV REQUIRED COMPONENTS core imgproc calib3d imgcodecs)

include_directories(${PROJECT_SOURCE_DIR}/include)

add_library(virtual_camera_core
    src/json_utils.cpp
    src/task_builder.cpp
    src/map_generator.cpp
    src/json_writer.cpp
    src/verifier.cpp
)

target_link_libraries(virtual_camera_core PUBLIC Eigen3::Eigen ${OpenCV_LIBS})

add_executable(virtual_camera_tool src/main.cpp)
target_link_libraries(virtual_camera_tool PRIVATE virtual_camera_core)

add_executable(test_task_builder tests/test_task_builder.cpp)
target_link_libraries(test_task_builder PRIVATE virtual_camera_core)

add_executable(test_verifier tests/test_verifier.cpp)
target_link_libraries(test_verifier PRIVATE virtual_camera_core)
```

- [ ] **Step 4: Write build script**

Create `scripts/build.sh`:

```bash
#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."
rm -rf build
cmake -S . -B build
cmake --build build -j"$(nproc)"
```

- [ ] **Step 5: Write run script**

Create `scripts/run_thor_verify.sh`:

```bash
#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

input_root="/workspace/L022/cfg/7v"
config_path="/workspace/VirtualCamera/configs/config_thor.json"
output_root="/workspace/VirtualCamera/output_verify/7v"

bash scripts/build.sh
rm -rf "$output_root"
./build/virtual_camera_tool generate-verify "$input_root" "$config_path" "$output_root"
```

- [ ] **Step 6: Mark scripts executable and build**

Run:

```bash
chmod +x scripts/build.sh scripts/run_thor_verify.sh
bash scripts/build.sh
```

Expected: CMake fails because source files do not exist yet.

- [ ] **Step 7: Commit scaffold**

```bash
git add .gitignore README.md CMakeLists.txt scripts/build.sh scripts/run_thor_verify.sh
git commit -m "chore: scaffold x86 virtual camera project"
```

## Task 2: Shared Types And JSON Utilities

**Files:**
- Create: `include/virtual_camera/types.h`
- Create: `include/virtual_camera/json_utils.h`
- Create: `src/json_utils.cpp`
- Test: `tests/test_task_builder.cpp`

- [ ] **Step 1: Write a failing JSON utility smoke test**

Create `tests/test_task_builder.cpp`:

```cpp
#include "virtual_camera/json_utils.h"

#include <filesystem>
#include <fstream>
#include <iostream>

int main() {
  const std::filesystem::path path = "build/test_json_utils.json";
  std::ofstream out(path);
  out << R"({"matrix":[[1,2,3],[4,5,6],[7,8,9]],"dist":[[0.1,0.2,0.3,0.4]]})";
  out.close();

  const auto json = vc::ReadJson(path.string());
  const auto matrix = vc::JsonToMatrix3d(json.at("matrix"));
  const auto dist = vc::JsonToVector(json.at("dist"));

  if (matrix(0, 0) != 1.0 || matrix(2, 2) != 9.0) {
    std::cerr << "matrix conversion failed\n";
    return 1;
  }
  if (dist.size() != 4 || dist[2] != 0.3) {
    std::cerr << "vector conversion failed\n";
    return 1;
  }
  return 0;
}
```

- [ ] **Step 2: Run failing test**

Run:

```bash
bash scripts/build.sh
```

Expected: FAIL with missing `virtual_camera/json_utils.h`.

- [ ] **Step 3: Add shared types**

Create `include/virtual_camera/types.h`:

```cpp
#pragma once

#include <Eigen/Dense>

#include <string>
#include <unordered_map>
#include <vector>

namespace vc {

enum class TaskType {
  kVirtual,
  kUndistort,
  kResize,
};

struct CalibrationParam {
  Eigen::Matrix3d intrinsic_matrix = Eigen::Matrix3d::Identity();
  Eigen::Matrix4d extrinsic_matrix = Eigen::Matrix4d::Identity();
  std::vector<double> dist_data;
  int image_width = 0;
  int image_height = 0;
  int fov = 0;
};

struct VirtualParam {
  int virtual_width = 0;
  int virtual_height = 0;
  double virtual_fov = 0.0;
  double virtual_fu = 0.0;
  double virtual_fv = 0.0;
  double virtual_cx = 0.0;
  double virtual_cy = 0.0;
  double virtual_yaw = 0.0;
  double virtual_pitch = 0.0;
  double virtual_roll = 0.0;
  double tx = 0.0;
  double ty = 0.0;
  double tz = 0.0;
  int image_width = 0;
  int image_height = 0;
  double fov = 0.0;
  int camera_id = 0;
  std::string bin_name;
};

struct CameraTask {
  TaskType type = TaskType::kVirtual;
  std::string description;
  std::string conf_json;
  std::string intri_json;
  std::string extri_json;
  std::string bin_name;
  std::string undistort_bin_name;
  int camera_id = 0;
  int image_width = 0;
  int image_height = 0;
  int source_fov = 0;
  VirtualParam virtual_param;
};

inline const std::unordered_map<int, std::string> kCameraNames = {
    {0, "camera-front-narrow"},
    {1, "camera-front-wide"},
    {2, "camera-left-front"},
    {3, "camera-left-back"},
    {4, "camera-right-front"},
    {5, "camera-right-back"},
    {6, "camera-back"},
};

}  // namespace vc
```

- [ ] **Step 4: Add JSON utility header**

Create `include/virtual_camera/json_utils.h`:

```cpp
#pragma once

#include <Eigen/Dense>
#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace vc {

nlohmann::json ReadJson(const std::string& file_path);
void WriteJson(const std::string& file_path, const nlohmann::json& json);
bool FileExists(const std::string& file_path);
void EnsureDirectory(const std::string& dir_path);
Eigen::Matrix4d JsonToMatrix4d(const nlohmann::json& data);
Eigen::Matrix3d JsonToMatrix3d(const nlohmann::json& data);
std::vector<double> JsonToVector(const nlohmann::json& data);

}  // namespace vc
```

- [ ] **Step 5: Add JSON utility implementation**

Create `src/json_utils.cpp`:

```cpp
#include "virtual_camera/json_utils.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace vc {

nlohmann::json ReadJson(const std::string& file_path) {
  std::ifstream input(file_path);
  if (!input.is_open()) {
    throw std::runtime_error("failed to open json file: " + file_path);
  }
  nlohmann::json json;
  input >> json;
  return json;
}

void WriteJson(const std::string& file_path, const nlohmann::json& json) {
  const auto parent = std::filesystem::path(file_path).parent_path();
  if (!parent.empty()) {
    EnsureDirectory(parent.string());
  }
  std::ofstream output(file_path, std::ios::binary);
  if (!output.is_open()) {
    throw std::runtime_error("failed to write json file: " + file_path);
  }
  output << json.dump(4) << '\n';
}

bool FileExists(const std::string& file_path) {
  return std::filesystem::is_regular_file(file_path);
}

void EnsureDirectory(const std::string& dir_path) {
  std::filesystem::create_directories(dir_path);
  if (!std::filesystem::is_directory(dir_path)) {
    throw std::runtime_error("failed to create directory: " + dir_path);
  }
}

Eigen::Matrix4d JsonToMatrix4d(const nlohmann::json& data) {
  Eigen::Matrix4d matrix = Eigen::Matrix4d::Identity();
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col) {
      matrix(row, col) = data.at(row).at(col).get<double>();
    }
  }
  return matrix;
}

Eigen::Matrix3d JsonToMatrix3d(const nlohmann::json& data) {
  Eigen::Matrix3d matrix = Eigen::Matrix3d::Identity();
  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 3; ++col) {
      matrix(row, col) = data.at(row).at(col).get<double>();
    }
  }
  return matrix;
}

std::vector<double> JsonToVector(const nlohmann::json& data) {
  const nlohmann::json* row = &data;
  if (data.is_array() && !data.empty() && data.at(0).is_array()) {
    row = &data.at(0);
  }
  std::vector<double> values;
  values.reserve(row->size());
  for (const auto& value : *row) {
    values.push_back(value.get<double>());
  }
  return values;
}

}  // namespace vc
```

- [ ] **Step 6: Run test**

Run:

```bash
bash scripts/build.sh
./build/test_task_builder
```

Expected: PASS with exit code 0.

- [ ] **Step 7: Commit utilities**

```bash
git add include/virtual_camera/types.h include/virtual_camera/json_utils.h src/json_utils.cpp tests/test_task_builder.cpp
git commit -m "feat: add shared json utilities"
```

## Task 3: Thor Config And Task Builder

**Files:**
- Create: `configs/config_thor.json`
- Create: `include/virtual_camera/task_builder.h`
- Create: `src/task_builder.cpp`
- Modify: `tests/test_task_builder.cpp`

- [ ] **Step 1: Copy Thor config into the new repository**

Run from `/workspace/VirtualCamera`:

```bash
mkdir -p configs
cp /workspace/icv_vc_bin_lib/configs/config_thor.json configs/config_thor.json
```

Expected: `configs/config_thor.json` exists in the new repository. This reads the reference project but does not modify it.

- [ ] **Step 2: Replace task builder test with config expansion checks**

Replace `tests/test_task_builder.cpp`:

```cpp
#include "virtual_camera/json_utils.h"
#include "virtual_camera/task_builder.h"

#include <iostream>
#include <set>

int main() {
  const auto config = vc::ReadJson("configs/config_thor.json");
  const auto tasks = vc::BuildThorTasks(config);

  int virtual_count = 0;
  int undistort_count = 0;
  int resize_count = 0;
  std::set<std::string> bin_names;

  for (const auto& task : tasks) {
    bin_names.insert(task.bin_name);
    if (task.type == vc::TaskType::kVirtual) {
      ++virtual_count;
    } else if (task.type == vc::TaskType::kUndistort) {
      ++undistort_count;
    } else if (task.type == vc::TaskType::kResize) {
      ++resize_count;
    }
  }

  if (virtual_count != 8) {
    std::cerr << "expected 8 virtual tasks, got " << virtual_count << "\n";
    return 1;
  }
  if (undistort_count != 7) {
    std::cerr << "expected 7 undistort tasks, got " << undistort_count << "\n";
    return 1;
  }
  if (resize_count != 3) {
    std::cerr << "expected 3 resize tasks, got " << resize_count << "\n";
    return 1;
  }
  if (!bin_names.count("gdc_ft30.bin") || !bin_names.count("gdc_fw120_1080.bin")) {
    std::cerr << "expected Thor bin names are missing\n";
    return 1;
  }
  return 0;
}
```

- [ ] **Step 3: Run failing test**

Run:

```bash
bash scripts/build.sh
./build/test_task_builder
```

Expected: FAIL with missing `virtual_camera/task_builder.h`.

- [ ] **Step 4: Add task builder header**

Create `include/virtual_camera/task_builder.h`:

```cpp
#pragma once

#include "virtual_camera/types.h"

#include <nlohmann/json.hpp>

#include <vector>

namespace vc {

std::vector<CameraTask> BuildThorTasks(const nlohmann::json& config);
CalibrationParam LoadCalibration(const std::string& input_root, const CameraTask& task);

}  // namespace vc
```

- [ ] **Step 5: Add task builder implementation**

Create `src/task_builder.cpp`:

```cpp
#include "virtual_camera/task_builder.h"

#include "virtual_camera/json_utils.h"

#include <filesystem>
#include <set>
#include <stdexcept>

namespace vc {
namespace {

VirtualParam ParseVirtualParam(const nlohmann::json& camera) {
  VirtualParam param;
  param.virtual_width = camera.at("new_intrinsic").at("image_width").get<int>();
  param.virtual_height = camera.at("new_intrinsic").at("image_height").get<int>();
  param.virtual_fov = camera.at("new_intrinsic").at("fov").get<double>();
  param.virtual_fu = camera.at("new_intrinsic").at("focal_u").get<double>();
  param.virtual_fv = camera.at("new_intrinsic").at("focal_v").get<double>();
  param.virtual_cx = camera.at("new_intrinsic").at("center_u").get<double>();
  param.virtual_cy = camera.at("new_intrinsic").at("center_v").get<double>();
  param.virtual_yaw = camera.at("new_extrinsics").at("yaw").get<double>();
  param.virtual_roll = camera.at("new_extrinsics").at("roll").get<double>();
  param.virtual_pitch = camera.at("new_extrinsics").at("pitch").get<double>();
  param.image_width = camera.at("image_width").get<int>();
  param.image_height = camera.at("image_height").get<int>();
  param.fov = camera.at("fov").get<double>();
  param.camera_id = camera.at("camera_id").get<int>();
  param.bin_name = camera.at("bin_name").get<std::string>();
  return param;
}

CameraTask ParseTask(const nlohmann::json& camera, TaskType type) {
  CameraTask task;
  task.type = type;
  task.description = camera.value("desc", std::string());
  task.conf_json = camera.at("conf_json").get<std::string>();
  task.intri_json = camera.at("intri_json").get<std::string>();
  task.extri_json = camera.at("extri_json").get<std::string>();
  task.bin_name = camera.at("bin_name").get<std::string>();
  task.undistort_bin_name = camera.value("undis_bin_name", std::string());
  task.camera_id = camera.at("camera_id").get<int>();
  task.image_width = camera.at("image_width").get<int>();
  task.image_height = camera.at("image_height").get<int>();
  task.source_fov = camera.at("fov").get<int>();
  task.virtual_param = ParseVirtualParam(camera);
  return task;
}

}  // namespace

std::vector<CameraTask> BuildThorTasks(const nlohmann::json& config) {
  std::vector<CameraTask> tasks;
  std::set<int> undistort_camera_ids;

  for (const auto& camera : config.at("virtual_camera_configs")) {
    tasks.push_back(ParseTask(camera, TaskType::kVirtual));
    const int camera_id = camera.at("camera_id").get<int>();
    if (undistort_camera_ids.insert(camera_id).second) {
      CameraTask undistort_task = ParseTask(camera, TaskType::kUndistort);
      undistort_task.bin_name = camera.at("undis_bin_name").get<std::string>();
      tasks.push_back(undistort_task);
    }
  }

  for (const auto& camera : config.at("virtual_init_camera_config")) {
    tasks.push_back(ParseTask(camera, TaskType::kResize));
  }

  return tasks;
}

CalibrationParam LoadCalibration(const std::string& input_root, const CameraTask& task) {
  const std::filesystem::path calib_root =
      std::filesystem::path(input_root) / "calib";
  const auto intri_json = ReadJson((calib_root / task.intri_json).string());
  const auto extri_json = ReadJson((calib_root / task.extri_json).string());

  CalibrationParam calibration;
  calibration.extrinsic_matrix =
      JsonToMatrix4d(extri_json.at("value0").at("param").at("sensor_calib").at("data"));
  calibration.intrinsic_matrix =
      JsonToMatrix3d(intri_json.at("value0").at("param").at("cam_K").at("data"));
  calibration.dist_data =
      JsonToVector(intri_json.at("value0").at("param").at("cam_dist").at("data"));
  calibration.image_width = task.image_width;
  calibration.image_height = task.image_height;
  calibration.fov = task.source_fov;
  return calibration;
}

}  // namespace vc
```

- [ ] **Step 6: Run task builder test**

Run:

```bash
bash scripts/build.sh
./build/test_task_builder
```

Expected: PASS.

- [ ] **Step 7: Commit task builder**

```bash
git add configs/config_thor.json include/virtual_camera/task_builder.h src/task_builder.cpp tests/test_task_builder.cpp
git commit -m "feat: expand thor config into camera tasks"
```

## Task 4: Verifier

**Files:**
- Create: `include/virtual_camera/verifier.h`
- Create: `src/verifier.cpp`
- Modify: `tests/test_verifier.cpp`

- [ ] **Step 1: Write verifier tests**

Create `tests/test_verifier.cpp`:

```cpp
#include "virtual_camera/json_utils.h"
#include "virtual_camera/verifier.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace {

void WriteFile(const std::string& path, const std::string& data) {
  vc::EnsureDirectory(std::filesystem::path(path).parent_path().string());
  std::ofstream output(path, std::ios::binary);
  output << data;
}

}  // namespace

int main() {
  const std::filesystem::path root = "build/test_verifier";
  std::filesystem::remove_all(root);

  WriteFile((root / "golden/calib/gdc/a.bin").string(), "abc");
  WriteFile((root / "actual/calib/gdc/a.bin").string(), "abc");
  WriteFile((root / "golden/calib/gdc_intri/b.bin").string(), "def");
  WriteFile((root / "actual/calib/gdc_intri/b.bin").string(), "def");

  nlohmann::json golden_json = {
      {"value0",
       {{"param",
         {{"cam_K", {{"data", {{1, 0, 2}, {0, 1, 3}, {0, 0, 1}}}}},
          {"cam_dist", {{"data", {{0.1, 0.2, 0.3, 0.4}}}}},
          {"img_new_w", 1024},
          {"img_new_h", 512},
          {"sensor_calib", {{"data", {{1, 0, 0, 1}, {0, 1, 0, 2}, {0, 0, 1, 3}, {0, 0, 0, 1}}}}}}}}}};
  vc::WriteJson((root / "golden/calib/virtual/cam/cam-intrinsic.json").string(), golden_json);
  vc::WriteJson((root / "actual/calib/virtual/cam/cam-intrinsic.json").string(), golden_json);

  const auto result = vc::VerifyOutputs((root / "golden").string(), (root / "actual").string());
  if (!result.ok) {
    std::cerr << result.message << "\n";
    return 1;
  }

  WriteFile((root / "actual/calib/gdc/a.bin").string(), "abx");
  const auto failed = vc::VerifyOutputs((root / "golden").string(), (root / "actual").string());
  if (failed.ok || failed.message.find("byte mismatch") == std::string::npos) {
    std::cerr << "expected byte mismatch failure\n";
    return 1;
  }

  return 0;
}
```

- [ ] **Step 2: Run failing verifier test**

Run:

```bash
bash scripts/build.sh
./build/test_verifier
```

Expected: FAIL with missing `virtual_camera/verifier.h`.

- [ ] **Step 3: Add verifier header**

Create `include/virtual_camera/verifier.h`:

```cpp
#pragma once

#include <string>

namespace vc {

struct VerifyResult {
  bool ok = false;
  std::string message;
};

VerifyResult VerifyOutputs(const std::string& golden_root, const std::string& actual_root);

}  // namespace vc
```

- [ ] **Step 4: Add verifier implementation**

Create `src/verifier.cpp`:

```cpp
#include "virtual_camera/verifier.h"

#include "virtual_camera/json_utils.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace vc {
namespace {

std::vector<std::filesystem::path> ListRelativeFiles(const std::filesystem::path& root,
                                                     const std::string& extension) {
  std::vector<std::filesystem::path> files;
  if (!std::filesystem::exists(root)) {
    return files;
  }
  for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
    if (entry.is_regular_file() && entry.path().extension() == extension) {
      files.push_back(std::filesystem::relative(entry.path(), root));
    }
  }
  std::sort(files.begin(), files.end());
  return files;
}

bool SameBytes(const std::filesystem::path& left, const std::filesystem::path& right,
               std::uintmax_t* offset) {
  if (std::filesystem::file_size(left) != std::filesystem::file_size(right)) {
    *offset = std::min(std::filesystem::file_size(left), std::filesystem::file_size(right));
    return false;
  }

  std::ifstream a(left, std::ios::binary);
  std::ifstream b(right, std::ios::binary);
  char ca = 0;
  char cb = 0;
  std::uintmax_t index = 0;
  while (a.get(ca) && b.get(cb)) {
    if (ca != cb) {
      *offset = index;
      return false;
    }
    ++index;
  }
  return true;
}

void CompareJsonValue(const nlohmann::json& golden, const nlohmann::json& actual,
                      const std::string& path, std::ostringstream* errors) {
  if (!golden.contains(json::json_pointer(path)) || !actual.contains(json::json_pointer(path))) {
    *errors << "json key missing: " << path << "\n";
    return;
  }
  const auto& g = golden.at(nlohmann::json::json_pointer(path));
  const auto& a = actual.at(nlohmann::json::json_pointer(path));
  if (g != a) {
    *errors << "json field mismatch: " << path << "\n";
  }
}

void CompareVirtualJson(const std::filesystem::path& golden_root,
                        const std::filesystem::path& actual_root,
                        std::ostringstream* errors) {
  const auto golden_files = ListRelativeFiles(golden_root / "calib/virtual", ".json");
  for (const auto& rel : golden_files) {
    const auto golden_path = golden_root / "calib/virtual" / rel;
    const auto actual_path = actual_root / "calib/virtual" / rel;
    if (!std::filesystem::exists(actual_path)) {
      *errors << "missing json: " << rel.string() << "\n";
      continue;
    }
    const auto golden = ReadJson(golden_path.string());
    const auto actual = ReadJson(actual_path.string());
    CompareJsonValue(golden, actual, "/value0/param/cam_K/data", errors);
    CompareJsonValue(golden, actual, "/value0/param/cam_K_new/data", errors);
    CompareJsonValue(golden, actual, "/value0/param/cam_dist/data", errors);
    CompareJsonValue(golden, actual, "/value0/param/img_new_w", errors);
    CompareJsonValue(golden, actual, "/value0/param/img_new_h", errors);
    CompareJsonValue(golden, actual, "/value0/param/sensor_calib/data", errors);
  }
}

void CompareBinDirectory(const std::filesystem::path& golden_root,
                         const std::filesystem::path& actual_root,
                         const std::string& subdir,
                         std::ostringstream* errors) {
  const auto golden_files = ListRelativeFiles(golden_root / subdir, ".bin");
  const auto actual_files = ListRelativeFiles(actual_root / subdir, ".bin");
  if (golden_files != actual_files) {
    *errors << "file set mismatch: " << subdir << "\n";
    return;
  }
  for (const auto& rel : golden_files) {
    std::uintmax_t offset = 0;
    const auto golden_path = golden_root / subdir / rel;
    const auto actual_path = actual_root / subdir / rel;
    if (!SameBytes(golden_path, actual_path, &offset)) {
      *errors << "byte mismatch: " << subdir << "/" << rel.string()
              << " at offset " << offset << "\n";
    }
  }
}

}  // namespace

VerifyResult VerifyOutputs(const std::string& golden_root, const std::string& actual_root) {
  std::ostringstream errors;
  CompareBinDirectory(golden_root, actual_root, "calib/gdc", &errors);
  CompareBinDirectory(golden_root, actual_root, "calib/gdc_intri", &errors);
  CompareVirtualJson(golden_root, actual_root, &errors);

  const std::string message = errors.str();
  if (!message.empty()) {
    return {false, message};
  }
  return {true, "verification passed"};
}

}  // namespace vc
```

- [ ] **Step 5: Fix namespace typo in verifier implementation if build reports it**

If compiler reports `json` is not declared in `CompareJsonValue`, replace:

```cpp
golden.contains(json::json_pointer(path))
```

with:

```cpp
golden.contains(nlohmann::json::json_pointer(path))
```

and make the same replacement for `actual.contains(...)`.

- [ ] **Step 6: Run verifier test**

Run:

```bash
bash scripts/build.sh
./build/test_verifier
```

Expected: PASS.

- [ ] **Step 7: Commit verifier**

```bash
git add include/virtual_camera/verifier.h src/verifier.cpp tests/test_verifier.cpp
git commit -m "feat: add golden output verifier"
```

## Task 5: Map Generator And JSON Writer

**Files:**
- Create: `include/virtual_camera/map_generator.h`
- Create: `src/map_generator.cpp`
- Create: `include/virtual_camera/json_writer.h`
- Create: `src/json_writer.cpp`

- [ ] **Step 1: Add map generator header**

Create `include/virtual_camera/map_generator.h`:

```cpp
#pragma once

#include "virtual_camera/types.h"

#include <Eigen/Dense>
#include <opencv2/core.hpp>

#include <string>
#include <vector>

namespace vc {

class MapGenerator {
 public:
  MapGenerator(const CalibrationParam& calibration, const VirtualParam& virtual_param);

  std::vector<cv::Mat> CreateVirtualMap(bool fisheye_model = true) const;
  std::vector<cv::Mat> CreateUndistortMap() const;
  std::vector<cv::Mat> CreateResizeMap(Eigen::Matrix3d* resized_intrinsic) const;
  void SaveBin(const std::string& save_path, const std::vector<cv::Mat>& maps,
               int source_width, int source_height, int output_width, int output_height) const;

  const Eigen::Matrix3d& virtual_intrinsic() const { return virtual_intrinsic_; }
  const Eigen::Matrix4d& virtual_extrinsic() const { return virtual_extrinsic_; }

 private:
  Eigen::Matrix4d BuildVirtualExtrinsic() const;
  Eigen::Matrix3d BuildVirtualIntrinsic() const;
  cv::Mat BuildCameraMatrix() const;
  cv::Mat BuildDistortion(int count) const;

  CalibrationParam calibration_;
  VirtualParam virtual_param_;
  Eigen::Matrix3d virtual_intrinsic_;
  Eigen::Matrix4d virtual_extrinsic_;
};

}  // namespace vc
```

- [ ] **Step 2: Add map generator implementation**

Create `src/map_generator.cpp` with the algorithm ported from the reference project and Chinese comments for non-obvious transforms:

```cpp
#include "virtual_camera/map_generator.h"

#include "virtual_camera/json_utils.h"

#include <opencv2/calib3d.hpp>
#include <opencv2/core/eigen.hpp>

#include <cmath>
#include <fstream>
#include <stdexcept>

namespace vc {
namespace {

constexpr double kPi = 3.14159265358979323846;

}  // namespace

MapGenerator::MapGenerator(const CalibrationParam& calibration,
                           const VirtualParam& virtual_param)
    : calibration_(calibration),
      virtual_param_(virtual_param),
      virtual_intrinsic_(BuildVirtualIntrinsic()),
      virtual_extrinsic_(BuildVirtualExtrinsic()) {}

Eigen::Matrix4d MapGenerator::BuildVirtualExtrinsic() const {
  Eigen::Matrix4d extrinsic = Eigen::Matrix4d::Identity();
  double yaw = virtual_param_.virtual_yaw;
  double pitch = 0.0;
  double roll = virtual_param_.virtual_roll;

  // VCS 坐标系转换到 OpenCV 相机坐标系。
  yaw -= 90.0;
  roll = -roll;
  roll -= 90.0;

  yaw *= kPi / 180.0;
  pitch *= kPi / 180.0;
  roll *= kPi / 180.0;

  const double sin_yaw = std::sin(yaw);
  const double cos_yaw = std::cos(yaw);
  const double sin_pitch = std::sin(pitch);
  const double cos_pitch = std::cos(pitch);
  const double sin_roll = std::sin(roll);
  const double cos_roll = std::cos(roll);

  extrinsic(0, 0) = cos_roll * cos_pitch;
  extrinsic(0, 1) = cos_roll * sin_pitch * sin_yaw - sin_roll * cos_yaw;
  extrinsic(0, 2) = cos_roll * sin_pitch * cos_yaw + sin_roll * sin_yaw;
  extrinsic(1, 0) = sin_roll * cos_pitch;
  extrinsic(1, 1) = sin_roll * sin_pitch * sin_yaw + cos_roll * cos_yaw;
  extrinsic(1, 2) = sin_roll * sin_pitch * cos_yaw - cos_roll * sin_yaw;
  extrinsic(2, 0) = -sin_pitch;
  extrinsic(2, 1) = cos_pitch * sin_yaw;
  extrinsic(2, 2) = cos_pitch * cos_yaw;
  extrinsic(0, 3) = virtual_param_.tx;
  extrinsic(1, 3) = virtual_param_.ty;
  extrinsic(2, 3) = virtual_param_.tz;
  return extrinsic;
}

Eigen::Matrix3d MapGenerator::BuildVirtualIntrinsic() const {
  Eigen::Matrix3d intrinsic = Eigen::Matrix3d::Identity();
  intrinsic(0, 0) = virtual_param_.virtual_width / 2.0 /
                    std::tan(virtual_param_.virtual_fov * (kPi / 360.0));
  intrinsic(1, 1) = intrinsic(0, 0);
  intrinsic(0, 2) = virtual_param_.virtual_width / 2.0;
  intrinsic(1, 2) = virtual_param_.virtual_height / 2.0;
  return intrinsic;
}

cv::Mat MapGenerator::BuildCameraMatrix() const {
  cv::Mat camera_matrix = cv::Mat::zeros(3, 3, CV_64FC1);
  cv::eigen2cv(calibration_.intrinsic_matrix, camera_matrix);

  // 前视 8M 标定在 2M 输出链路中使用半尺度内参。
  if ((virtual_param_.camera_id == 0 || virtual_param_.camera_id == 1) &&
      calibration_.intrinsic_matrix(0, 2) > calibration_.image_width * 0.75) {
    camera_matrix.at<double>(0, 0) *= 0.5;
    camera_matrix.at<double>(0, 2) *= 0.5;
    camera_matrix.at<double>(1, 1) *= 0.5;
    camera_matrix.at<double>(1, 2) *= 0.5;
  }
  return camera_matrix;
}

cv::Mat MapGenerator::BuildDistortion(int count) const {
  std::vector<double> dist(calibration_.dist_data.begin(),
                           calibration_.dist_data.begin() + std::min<int>(count, calibration_.dist_data.size()));
  cv::Mat distortion(dist, true);
  distortion.convertTo(distortion, CV_32F);
  return distortion.reshape(0, count);
}

std::vector<cv::Mat> MapGenerator::CreateVirtualMap(bool fisheye_model) const {
  const Eigen::Matrix3d rotation =
      virtual_extrinsic_.block<3, 3>(0, 0).inverse() *
      calibration_.extrinsic_matrix.block<3, 3>(0, 0);

  cv::Mat camera_matrix = BuildCameraMatrix();
  cv::Mat distortion = BuildDistortion(fisheye_model ? 4 : 8);
  cv::Mat rotation_new = cv::Mat::zeros(3, 3, CV_64FC1);
  cv::Mat projection = cv::Mat::zeros(3, 3, CV_64FC1);
  cv::eigen2cv(rotation, rotation_new);
  cv::eigen2cv(virtual_intrinsic_, projection);

  cv::Mat map_x;
  cv::Mat map_y;
  if (fisheye_model) {
    cv::fisheye::initUndistortRectifyMap(
        camera_matrix, distortion, rotation_new, projection,
        cv::Size(virtual_param_.virtual_width, virtual_param_.virtual_height),
        CV_32FC1, map_x, map_y);
  } else {
    cv::initUndistortRectifyMap(
        camera_matrix, distortion, rotation_new, projection,
        cv::Size(virtual_param_.virtual_width, virtual_param_.virtual_height),
        CV_32FC1, map_x, map_y);
  }
  return {map_x, map_y};
}

std::vector<cv::Mat> MapGenerator::CreateUndistortMap() const {
  cv::Mat camera_matrix = BuildCameraMatrix();
  cv::Mat distortion = BuildDistortion(4);
  cv::Mat rotation_new = cv::Mat::eye(3, 3, CV_64FC1);
  cv::Mat projection = cv::Mat::zeros(3, 3, CV_64FC1);
  projection.at<double>(0, 0) =
      std::round(calibration_.image_width / 2.0 /
                 std::tan(calibration_.fov * kPi / 360.0) * 10.0) /
      10.0;
  projection.at<double>(1, 1) = projection.at<double>(0, 0);
  projection.at<double>(0, 2) = camera_matrix.at<double>(0, 2);
  projection.at<double>(1, 2) = camera_matrix.at<double>(1, 2);
  projection.at<double>(2, 2) = 1.0;

  cv::Mat map_x;
  cv::Mat map_y;
  cv::fisheye::initUndistortRectifyMap(
      camera_matrix, distortion, rotation_new, projection,
      cv::Size(calibration_.image_width, calibration_.image_height),
      CV_32FC1, map_x, map_y);
  return {map_x, map_y};
}

std::vector<cv::Mat> MapGenerator::CreateResizeMap(Eigen::Matrix3d* resized_intrinsic) const {
  cv::Mat camera_matrix = BuildCameraMatrix();
  cv::Mat distortion = BuildDistortion(4);
  cv::Mat rotation_new = cv::Mat::eye(3, 3, CV_64FC1);

  Eigen::Matrix3d intrinsic = Eigen::Matrix3d::Identity();
  intrinsic(0, 2) = camera_matrix.at<double>(0, 2) *
                    (static_cast<double>(virtual_param_.virtual_width) / calibration_.image_width);
  intrinsic(1, 2) = camera_matrix.at<double>(1, 2) *
                    (static_cast<double>(virtual_param_.virtual_height) / calibration_.image_height);
  intrinsic(0, 0) = camera_matrix.at<double>(0, 0) *
                    (static_cast<double>(virtual_param_.virtual_width) / calibration_.image_width) *
                    (std::tan(calibration_.fov * kPi / 360.0) /
                     std::tan(virtual_param_.virtual_fov * kPi / 360.0));
  intrinsic(1, 1) = camera_matrix.at<double>(1, 1) *
                    (static_cast<double>(virtual_param_.virtual_height) / calibration_.image_height) *
                    (std::tan(calibration_.fov * kPi / 360.0) /
                     std::tan(virtual_param_.virtual_fov * kPi / 360.0));
  if (resized_intrinsic != nullptr) {
    *resized_intrinsic = intrinsic;
  }

  cv::Mat projection = cv::Mat::zeros(3, 3, CV_64FC1);
  cv::eigen2cv(intrinsic, projection);

  cv::Mat map_x;
  cv::Mat map_y;
  cv::fisheye::initUndistortRectifyMap(
      camera_matrix, distortion, rotation_new, projection,
      cv::Size(virtual_param_.virtual_width, virtual_param_.virtual_height),
      CV_32FC1, map_x, map_y);
  return {map_x, map_y};
}

void MapGenerator::SaveBin(const std::string& save_path, const std::vector<cv::Mat>& maps,
                           int source_width, int source_height,
                           int output_width, int output_height) const {
  EnsureDirectory(std::filesystem::path(save_path).parent_path().string());
  cv::Mat merged;
  cv::merge(maps, merged);
  for (int row = 0; row < output_height; ++row) {
    for (int col = 0; col < output_width; ++col) {
      const float x = merged.at<cv::Vec2f>(row, col)[0];
      const float y = merged.at<cv::Vec2f>(row, col)[1];
      if (x < 0 || y < 0 || x > source_width - 1 || y > source_height - 1) {
        merged.at<cv::Vec2f>(row, col) = cv::Vec2f(0.0f, 0.0f);
      }
    }
  }

  std::ofstream output(save_path, std::ios::binary);
  if (!output.is_open()) {
    throw std::runtime_error("failed to write bin: " + save_path);
  }
  output.write(reinterpret_cast<const char*>(merged.data),
               static_cast<std::streamsize>(output_width * output_height * sizeof(float) * 2));
}

}  // namespace vc
```

- [ ] **Step 3: Add JSON writer header**

Create `include/virtual_camera/json_writer.h`:

```cpp
#pragma once

#include "virtual_camera/types.h"

#include <Eigen/Dense>

#include <string>

namespace vc {

void SaveVirtualJson(const std::string& input_root, const std::string& output_root,
                     const CameraTask& task, const Eigen::Matrix4d& virtual_extrinsic,
                     const Eigen::Matrix3d& virtual_intrinsic,
                     const std::vector<double>& dist_data);

}  // namespace vc
```

- [ ] **Step 4: Add JSON writer implementation**

Create `src/json_writer.cpp`:

```cpp
#include "virtual_camera/json_writer.h"

#include "virtual_camera/json_utils.h"

#include <filesystem>

namespace vc {

void SaveVirtualJson(const std::string& input_root, const std::string& output_root,
                     const CameraTask& task, const Eigen::Matrix4d& virtual_extrinsic,
                     const Eigen::Matrix3d& virtual_intrinsic,
                     const std::vector<double>& dist_data) {
  const std::filesystem::path input_calib = std::filesystem::path(input_root) / "calib";
  const auto intri_template = ReadJson((input_calib / task.intri_json).string());
  const auto extri_template = ReadJson((input_calib / task.extri_json).string());

  nlohmann::json intri_json = intri_template;
  nlohmann::json extri_json = extri_template;
  const std::string camera_name = task.conf_json;

  auto& extri_data = extri_json["value0"]["param"]["sensor_calib"]["data"];
  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 3; ++col) {
      extri_data[row][col] = virtual_extrinsic(row, col);
    }
  }
  extri_json["value0"]["sensor_name"] = camera_name;

  intri_json["value0"]["param"]["img_new_w"] = task.virtual_param.virtual_width;
  intri_json["value0"]["param"]["img_new_h"] = task.virtual_param.virtual_height;

  auto& new_k = intri_json["value0"]["param"]["cam_K_new"]["data"];
  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 3; ++col) {
      new_k[row][col] = virtual_intrinsic(row, col);
    }
  }

  intri_json["value0"]["param"]["cam_dist"]["cols"] = dist_data.size();
  auto& dist = intri_json["value0"]["param"]["cam_dist"]["data"][0];
  dist = nlohmann::json::array();
  for (double value : dist_data) {
    dist.push_back(value);
  }
  intri_json["value0"]["sensor_name"] = camera_name;
  intri_json["value0"]["target_sensor_name"] = camera_name;

  const std::filesystem::path output_dir =
      std::filesystem::path(output_root) / "calib/virtual" / camera_name;
  WriteJson((output_dir / (camera_name + "-intrinsic.json")).string(), intri_json);
  WriteJson((output_dir / (camera_name + "-to-car_center-extrinsic.json")).string(), extri_json);
}

}  // namespace vc
```

- [ ] **Step 5: Build**

Run:

```bash
bash scripts/build.sh
```

Expected: PASS. If OpenCV include paths differ, inspect `cmake --find-package` output and adjust only the new repository CMake file.

- [ ] **Step 6: Commit map generator**

```bash
git add include/virtual_camera/map_generator.h src/map_generator.cpp include/virtual_camera/json_writer.h src/json_writer.cpp
git commit -m "feat: add map generation and virtual json writer"
```

## Task 6: CLI Generate And Verify Flow

**Files:**
- Create: `src/main.cpp`

- [ ] **Step 1: Add CLI implementation**

Create `src/main.cpp`:

```cpp
#include "virtual_camera/json_utils.h"
#include "virtual_camera/json_writer.h"
#include "virtual_camera/map_generator.h"
#include "virtual_camera/task_builder.h"
#include "virtual_camera/verifier.h"

#include <filesystem>
#include <iostream>

namespace {

int GenerateVerify(const std::string& input_root, const std::string& config_path,
                   const std::string& output_root) {
  const auto config = vc::ReadJson(config_path);
  const auto tasks = vc::BuildThorTasks(config);

  for (const auto& task : tasks) {
    const auto calibration = vc::LoadCalibration(input_root, task);
    vc::MapGenerator generator(calibration, task.virtual_param);

    if (task.type == vc::TaskType::kVirtual) {
      const auto maps = generator.CreateVirtualMap(true);
      const auto save_path = std::filesystem::path(output_root) / "calib/gdc" / task.bin_name;
      generator.SaveBin(save_path.string(), maps, calibration.image_width, calibration.image_height,
                        task.virtual_param.virtual_width, task.virtual_param.virtual_height);
      vc::SaveVirtualJson(input_root, output_root, task, generator.virtual_extrinsic(),
                          generator.virtual_intrinsic(), calibration.dist_data);
    } else if (task.type == vc::TaskType::kUndistort) {
      const auto maps = generator.CreateUndistortMap();
      const auto save_path = std::filesystem::path(output_root) / "calib/gdc_intri" / task.bin_name;
      generator.SaveBin(save_path.string(), maps, calibration.image_width, calibration.image_height,
                        calibration.image_width, calibration.image_height);
    } else if (task.type == vc::TaskType::kResize) {
      Eigen::Matrix3d resized_intrinsic = Eigen::Matrix3d::Identity();
      const auto maps = generator.CreateResizeMap(&resized_intrinsic);
      const auto save_path = std::filesystem::path(output_root) / "calib/gdc" / task.bin_name;
      generator.SaveBin(save_path.string(), maps, calibration.image_width, calibration.image_height,
                        task.virtual_param.virtual_width, task.virtual_param.virtual_height);
      vc::SaveVirtualJson(input_root, output_root, task, calibration.extrinsic_matrix,
                          resized_intrinsic, calibration.dist_data);
    }
  }

  const auto result = vc::VerifyOutputs(input_root, output_root);
  if (!result.ok) {
    std::cerr << result.message;
    return 2;
  }
  std::cout << result.message << "\n";
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 5 || std::string(argv[1]) != "generate-verify") {
    std::cerr << "Usage: " << argv[0]
              << " generate-verify <input_root> <config_path> <output_root>\n";
    return 1;
  }

  try {
    return GenerateVerify(argv[2], argv[3], argv[4]);
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << "\n";
    return 1;
  }
}
```

- [ ] **Step 2: Build and run unit checks**

Run:

```bash
bash scripts/build.sh
./build/test_task_builder
./build/test_verifier
```

Expected: both tests PASS.

- [ ] **Step 3: Run full generate/verify**

Run:

```bash
bash scripts/run_thor_verify.sh
```

Expected: If the port is already byte-identical, PASS with `verification passed`. If not, FAIL listing mismatched bin files and offsets.

- [ ] **Step 4: Commit CLI**

If Step 3 builds and produces either a clear verifier failure or a pass, commit the executable flow:

```bash
git add src/main.cpp
git commit -m "feat: add thor generate verify cli"
```

## Task 7: Golden Difference Debug Loop

**Files:**
- Modify: `src/map_generator.cpp`
- Modify: `src/json_writer.cpp`
- Modify: `include/virtual_camera/verifier.h`
- Modify: `src/verifier.cpp`

- [ ] **Step 1: Run full verifier and capture first failing file**

Run:

```bash
bash scripts/run_thor_verify.sh 2>&1 | tee build/verify.log
sed -n '1,80p' build/verify.log
```

Expected when failing: first mismatch line in the form `byte mismatch: calib/gdc/<file>.bin at offset <n>` or `byte mismatch: calib/gdc_intri/<file>.bin at offset <n>`.

- [ ] **Step 2: Inspect sizes and md5 values**

Run:

```bash
gold="/workspace/L022/cfg/7v"
out="/workspace/VirtualCamera/output_verify/7v"
find "$gold/calib/gdc" "$gold/calib/gdc_intri" -name '*.bin' -printf '%p %s\n' | sort > build/golden_bins.txt
find "$out/calib/gdc" "$out/calib/gdc_intri" -name '*.bin' -printf '%p %s\n' | sort > build/output_bins.txt
md5sum "$gold/calib/gdc/gdc_ft30.bin" "$out/calib/gdc/gdc_ft30.bin"
```

Expected: file sizes match. If sizes differ, fix task dimensions before algorithm changes.

- [ ] **Step 3: Fix task dimension mismatch if present**

If size output shows a task dimension mismatch, inspect the task fields:

```bash
./build/virtual_camera_tool generate-verify /workspace/L022/cfg/7v configs/config_thor.json output_verify/7v
```

Then update `src/task_builder.cpp` so each task uses:

```cpp
task.image_width = camera.at("image_width").get<int>();
task.image_height = camera.at("image_height").get<int>();
task.virtual_param.virtual_width = camera.at("new_intrinsic").at("image_width").get<int>();
task.virtual_param.virtual_height = camera.at("new_intrinsic").at("image_height").get<int>();
```

Run `bash scripts/run_thor_verify.sh` again.

- [ ] **Step 4: Fix algorithm mismatch only after dimensions match**

If dimensions match and bytes differ, compare `src/map_generator.cpp` against the reference algorithm by reading the reference files without modifying them:

```bash
sed -n '1,420p' /workspace/icv_vc_bin_lib/app/create_bin.cpp > build/reference_create_bin.cpp
diff -u build/reference_create_bin.cpp src/map_generator.cpp | sed -n '1,220p'
```

Make the smallest change in `src/map_generator.cpp` that restores reference behavior for the mismatched task. Keep Chinese comments near coordinate-system or scaling logic.

- [ ] **Step 5: Run full verifier after each algorithm change**

Run:

```bash
bash scripts/run_thor_verify.sh
```

Expected: Either PASS or a smaller set of mismatches. Repeat Steps 1-5 until `gdc` and `gdc_intri` bins pass byte-for-byte.

- [ ] **Step 6: Commit each isolated fix**

For each verified fix:

```bash
git add src/map_generator.cpp src/task_builder.cpp
git commit -m "fix: match thor golden map output"
```

## Task 8: Final Verification And GitHub Push

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Update README with final verified commands**

Append to `README.md`:

```markdown
## Verification Result

The expected verification command is:

```bash
bash scripts/run_thor_verify.sh
```

A passing run prints:

```text
verification passed
```
```

- [ ] **Step 2: Run all local verification**

Run:

```bash
bash scripts/build.sh
./build/test_task_builder
./build/test_verifier
bash scripts/run_thor_verify.sh
git status --short
```

Expected:

- Build succeeds.
- Both test executables return 0.
- Full verifier prints `verification passed`.
- `git status --short` shows only expected README changes before the final commit, and generated output remains ignored.

- [ ] **Step 3: Commit final docs**

```bash
git add README.md
git commit -m "docs: document thor verification workflow"
```

- [ ] **Step 4: Configure GitHub remote**

Run:

```bash
git remote add origin https://github.com/zoukai214/VirtualCamera
git remote -v
```

Expected: `origin` points to `https://github.com/zoukai214/VirtualCamera`.

- [ ] **Step 5: Push**

Run:

```bash
git push -u origin master
```

Expected: push succeeds. If authentication fails, report the failure and leave the local repository complete and committed.

## Self-Review

- Spec coverage: Tasks cover new repository setup, no modifications to `/workspace/icv_vc_bin_lib`, config-driven Thor tasks, x86 build, isolated output directory, bin byte verification, virtual JSON field verification, and GitHub push.
- Placeholder scan: The plan contains no unresolved implementation placeholders.
- Type consistency: `CameraTask`, `CalibrationParam`, `VirtualParam`, `MapGenerator`, `SaveVirtualJson`, and `VerifyOutputs` are introduced before later tasks use them.
