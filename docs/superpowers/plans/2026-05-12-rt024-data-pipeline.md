# RT024 Data Pipeline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将参考项目中的 `process_virtual_camera` 和 `process_undistort` 迁移到当前仓库，使用新的 `config.json` 单入口驱动 RT024 数据集的参数、映射表和图像输出，并校验虚拟相机映射表与参数输出。

**Architecture:** 保留当前仓库中可复用的底层映射生成能力和 JSON 工具，新增一层参考风格配置解析、标定加载、去畸变处理、虚拟相机处理与 RT024 专用校验。主入口替换为 `virtual_camera_tool <config.json>`，旧 Thor CLI 不再兼容。

**Tech Stack:** C++17, Eigen, OpenCV, nlohmann_json, CMake, existing `virtual_camera` core library

---

## File Structure

### New Files

- `include/virtual_camera/pipeline_config.h`
  - 新配置的数据结构与加载接口
- `src/pipeline_config.cpp`
  - 新配置 JSON 解析实现
- `include/virtual_camera/calibration_loader.h`
  - RT024 标定加载接口
- `src/calibration_loader.cpp`
  - 从 `calib_extract/*.json` 读取内外参与畸变参数
- `include/virtual_camera/undistort_processor.h`
  - 去畸变任务执行接口
- `src/undistort_processor.cpp`
  - 去畸变参数输出、图像 remap 与目录落盘
- `include/virtual_camera/virtual_camera_processor.h`
  - 虚拟相机任务执行接口
- `src/virtual_camera_processor.cpp`
  - 虚拟相机映射表、参数与图像输出
- `tests/test_pipeline_config.cpp`
  - 新配置解析测试
- `tests/test_calibration_loader.cpp`
  - 标定加载测试
- `tests/test_rt024_verifier.cpp`
  - RT024 输出校验测试
- `tests/test_rt024_json_writer.cpp`
  - 去畸变/虚拟相机参数 JSON 输出测试
- `configs/config_rt024.json`
  - RT024 默认运行配置

### Files To Modify

- `src/main.cpp`
  - 改为单入口 `virtual_camera_tool <config.json>`
- `include/virtual_camera/json_writer.h`
  - 增加 RT024 参数 JSON 输出接口
- `src/json_writer.cpp`
  - 增加去畸变/虚拟相机参数输出实现
- `include/virtual_camera/verifier.h`
  - 增加 RT024 校验接口
- `src/verifier.cpp`
  - 增加 bin/json 校验实现
- `include/virtual_camera/types.h`
  - 增加新流程需要的轻量结构体
- `CMakeLists.txt`
  - 仅增加新源文件与测试目标，不改编译选项
- `README.md`
  - 更新新的运行方式与 RT024 数据集说明

## Task 1: Define Pipeline Config Types

**Files:**
- Create: `include/virtual_camera/pipeline_config.h`
- Modify: `include/virtual_camera/types.h`
- Test: `tests/test_pipeline_config.cpp`

- [ ] **Step 1: Write the failing test**

```cpp
#include "virtual_camera/pipeline_config.h"

#include <fstream>
#include <stdexcept>

namespace {

void WriteText(const std::string& path, const std::string& content) {
  std::ofstream output(path);
  output << content;
}

void Expect(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void TestLoadPipelineConfigSuccess() {
  const std::string root = "build/test_tmp/pipeline_config_success";
  std::filesystem::create_directories(root);
  const std::string path = root + "/config.json";
  WriteText(path, R"json({
    "dataset_root": "/workspace/GACRT024_1754812994",
    "conf_dir_path": "calib_extract/",
    "image_dir_path": "image_raw/",
    "vc_image_dir_path": "image_virtual_camera/",
    "undistort_image_dir_path": "image_undistortion/",
    "vc_conf_dir_path": "calib_virtual_camera/",
    "undistort_conf_dir_path": "calib_undistortion/",
    "vc_gdcbin_dir_path": "vc_gdcbin_dir_path/",
    "showinfo": 1,
    "showdir": 0,
    "process_virtual_camera": 1,
    "process_undistort": 1,
    "undistort_image": 0,
    "distort_model": 0,
    "save_virtual_json": 1,
    "save_undistort_json": 1,
    "conf_type": 3,
    "virtual_camera_configs": [{
      "desc": "front wide -> front wide 110",
      "conf_json": "calib_camera_front_wide_to_car.json",
      "conf_intri_key": "camera-front-wide",
      "conf_extri_key": "camera-front-wide-to-car",
      "image_dir": "front_wide/",
      "save_dir": "front_wide_110/",
      "camera_id": 1,
      "image_width": 3840,
      "image_height": 2160,
      "fov": 120,
      "undistort_image": 1,
      "vc_mapX_name": "fw110_vc_mapX.bin",
      "vc_mapY_name": "fw110_vc_mapY.bin",
      "src2vc_mapX_name": "fw110_src2vc_mapX.bin",
      "src2vc_mapY_name": "fw110_src2vc_mapY.bin",
      "new_intrinsic": {
        "fov": 110,
        "focal_u": 358.5,
        "center_u": 512.0,
        "focal_v": 358.5,
        "center_v": 256.0,
        "image_width": 1024,
        "image_height": 512
      },
      "new_extrinsics": {
        "pitch": 0.0,
        "roll": 0.0,
        "yaw": 0.0,
        "x": 1.95,
        "y": -0.04,
        "z": 1.54
      }
    }],
    "undistort_configs": [{
      "conf_json": "calib_camera_front_wide_to_car.json",
      "intri_key": "camera-front-wide",
      "extri_key": "camera-front-wide-to-car",
      "image_dir": "front_wide/",
      "new_intrinsic": {
        "focal_u": 1000.0,
        "center_u": 1920.0,
        "focal_v": 1000.0,
        "center_v": 1080.0,
        "image_width": 3840,
        "image_height": 2160,
        "center": 1
      }
    }]
  })json");

  const vc::PipelineConfig config = vc::LoadPipelineConfig(path);
  Expect(config.dataset_root == "/workspace/GACRT024_1754812994", "dataset_root");
  Expect(config.virtual_tasks.size() == 1, "virtual task count");
  Expect(config.undistort_tasks.size() == 1, "undistort task count");
  Expect(config.virtual_tasks.front().save_dir == "front_wide_110/", "save_dir");
  Expect(config.virtual_tasks.front().src2vc_mapx_name == "fw110_src2vc_mapX.bin",
         "src2vc_mapx_name");
  Expect(config.undistort_tasks.front().image_dir == "front_wide/", "image_dir");
}

void TestLoadPipelineConfigMissingField() {
  const std::string root = "build/test_tmp/pipeline_config_missing";
  std::filesystem::create_directories(root);
  const std::string path = root + "/config.json";
  WriteText(path, R"json({
    "dataset_root": "/workspace/GACRT024_1754812994",
    "virtual_camera_configs": [],
    "undistort_configs": []
  })json");

  bool thrown = false;
  try {
    static_cast<void>(vc::LoadPipelineConfig(path));
  } catch (const std::runtime_error& error) {
    thrown = std::string(error.what()).find("conf_dir_path") != std::string::npos;
  }
  Expect(thrown, "missing field should mention conf_dir_path");
}

}  // namespace

int main() {
  TestLoadPipelineConfigSuccess();
  TestLoadPipelineConfigMissingField();
  return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target test_pipeline_config`

Expected: FAIL with missing target or missing header/source errors because `pipeline_config` does not exist yet.

- [ ] **Step 3: Write minimal implementation**

```cpp
// include/virtual_camera/pipeline_config.h
#pragma once

#include "virtual_camera/types.h"

#include <string>
#include <vector>

namespace vc {

struct OutputPathConfig {
  std::string dataset_root;
  std::string conf_dir_path;
  std::string image_dir_path;
  std::string vc_image_dir_path;
  std::string undistort_image_dir_path;
  std::string vc_conf_dir_path;
  std::string undistort_conf_dir_path;
  std::string vc_gdcbin_dir_path;
};

struct NewIntrinsicConfig {
  double fov = 0.0;
  double focal_u = 0.0;
  double center_u = 0.0;
  double focal_v = 0.0;
  double center_v = 0.0;
  int image_width = 0;
  int image_height = 0;
  int center = 1;
};

struct NewExtrinsicsConfig {
  double pitch = 0.0;
  double roll = 0.0;
  double yaw = 0.0;
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
};

struct VirtualCameraTaskConfig {
  std::string desc;
  std::string conf_json;
  std::string conf_intri_key;
  std::string conf_extri_key;
  std::string image_dir;
  std::string save_dir;
  std::string vc_mapx_name;
  std::string vc_mapy_name;
  std::string src2vc_mapx_name;
  std::string src2vc_mapy_name;
  int camera_id = 0;
  int image_width = 0;
  int image_height = 0;
  int fov = 0;
  int undistort_image = 0;
  NewIntrinsicConfig new_intrinsic;
  NewExtrinsicsConfig new_extrinsics;
};

struct UndistortTaskConfig {
  std::string conf_json;
  std::string intri_key;
  std::string extri_key;
  std::string image_dir;
  NewIntrinsicConfig new_intrinsic;
};

struct PipelineConfig {
  OutputPathConfig paths;
  int showinfo = 0;
  int showdir = 0;
  int process_virtual_camera = 0;
  int process_undistort = 0;
  int undistort_image = 0;
  int distort_model = 0;
  int save_virtual_json = 0;
  int save_undistort_json = 0;
  int conf_type = 0;
  std::string dataset_root;
  std::vector<VirtualCameraTaskConfig> virtual_tasks;
  std::vector<UndistortTaskConfig> undistort_tasks;
};

PipelineConfig LoadPipelineConfig(const std::string& config_path);

}  // namespace vc
```

```cpp
// src/pipeline_config.cpp
#include "virtual_camera/pipeline_config.h"

#include "virtual_camera/json_utils.h"

namespace vc {
namespace {

template <typename T>
T Required(const nlohmann::json& json, const char* key) {
  if (!json.contains(key)) {
    throw std::runtime_error(std::string("missing required field: ") + key);
  }
  return json.at(key).get<T>();
}

NewIntrinsicConfig ParseNewIntrinsic(const nlohmann::json& json) {
  NewIntrinsicConfig cfg;
  cfg.fov = json.value("fov", 0.0);
  cfg.focal_u = Required<double>(json, "focal_u");
  cfg.center_u = Required<double>(json, "center_u");
  cfg.focal_v = Required<double>(json, "focal_v");
  cfg.center_v = Required<double>(json, "center_v");
  cfg.image_width = Required<int>(json, "image_width");
  cfg.image_height = Required<int>(json, "image_height");
  cfg.center = json.value("center", 1);
  return cfg;
}

NewExtrinsicsConfig ParseNewExtrinsics(const nlohmann::json& json) {
  NewExtrinsicsConfig cfg;
  cfg.pitch = Required<double>(json, "pitch");
  cfg.roll = Required<double>(json, "roll");
  cfg.yaw = Required<double>(json, "yaw");
  cfg.x = Required<double>(json, "x");
  cfg.y = Required<double>(json, "y");
  cfg.z = Required<double>(json, "z");
  return cfg;
}

}  // namespace

PipelineConfig LoadPipelineConfig(const std::string& config_path) {
  const auto json = ReadJson(config_path);
  PipelineConfig cfg;
  cfg.dataset_root = Required<std::string>(json, "dataset_root");
  cfg.paths.dataset_root = cfg.dataset_root;
  cfg.paths.conf_dir_path = Required<std::string>(json, "conf_dir_path");
  cfg.paths.image_dir_path = Required<std::string>(json, "image_dir_path");
  cfg.paths.vc_image_dir_path = Required<std::string>(json, "vc_image_dir_path");
  cfg.paths.undistort_image_dir_path =
      Required<std::string>(json, "undistort_image_dir_path");
  cfg.paths.vc_conf_dir_path = Required<std::string>(json, "vc_conf_dir_path");
  cfg.paths.undistort_conf_dir_path =
      Required<std::string>(json, "undistort_conf_dir_path");
  cfg.paths.vc_gdcbin_dir_path = Required<std::string>(json, "vc_gdcbin_dir_path");
  cfg.showinfo = json.value("showinfo", 0);
  cfg.showdir = json.value("showdir", 0);
  cfg.process_virtual_camera = json.value("process_virtual_camera", 0);
  cfg.process_undistort = json.value("process_undistort", 0);
  cfg.undistort_image = json.value("undistort_image", 0);
  cfg.distort_model = json.value("distort_model", 0);
  cfg.save_virtual_json = json.value("save_virtual_json", 0);
  cfg.save_undistort_json = json.value("save_undistort_json", 0);
  cfg.conf_type = json.value("conf_type", 0);

  for (const auto& task_json : Required<nlohmann::json>(json, "virtual_camera_configs")) {
    VirtualCameraTaskConfig task;
    task.desc = task_json.value("desc", std::string());
    task.conf_json = Required<std::string>(task_json, "conf_json");
    task.conf_intri_key = Required<std::string>(task_json, "conf_intri_key");
    task.conf_extri_key = Required<std::string>(task_json, "conf_extri_key");
    task.image_dir = Required<std::string>(task_json, "image_dir");
    task.save_dir = Required<std::string>(task_json, "save_dir");
    task.vc_mapx_name = Required<std::string>(task_json, "vc_mapX_name");
    task.vc_mapy_name = Required<std::string>(task_json, "vc_mapY_name");
    task.src2vc_mapx_name = Required<std::string>(task_json, "src2vc_mapX_name");
    task.src2vc_mapy_name = Required<std::string>(task_json, "src2vc_mapY_name");
    task.camera_id = Required<int>(task_json, "camera_id");
    task.image_width = Required<int>(task_json, "image_width");
    task.image_height = Required<int>(task_json, "image_height");
    task.fov = Required<int>(task_json, "fov");
    task.undistort_image = task_json.value("undistort_image", 0);
    task.new_intrinsic = ParseNewIntrinsic(task_json.at("new_intrinsic"));
    task.new_extrinsics = ParseNewExtrinsics(task_json.at("new_extrinsics"));
    cfg.virtual_tasks.push_back(task);
  }

  for (const auto& task_json : Required<nlohmann::json>(json, "undistort_configs")) {
    UndistortTaskConfig task;
    task.conf_json = Required<std::string>(task_json, "conf_json");
    task.intri_key = Required<std::string>(task_json, "intri_key");
    task.extri_key = Required<std::string>(task_json, "extri_key");
    task.image_dir = Required<std::string>(task_json, "image_dir");
    task.new_intrinsic = ParseNewIntrinsic(task_json.at("new_intrinsic"));
    cfg.undistort_tasks.push_back(task);
  }
  return cfg;
}

}  // namespace vc
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --target test_pipeline_config && ./build/test_pipeline_config`

Expected: PASS with exit code `0`.

- [ ] **Step 5: Commit**

```bash
git add include/virtual_camera/types.h include/virtual_camera/pipeline_config.h src/pipeline_config.cpp tests/test_pipeline_config.cpp CMakeLists.txt
git commit -m "feat: add RT024 pipeline config parser"
```

## Task 2: Load RT024 Calibration Files

**Files:**
- Create: `include/virtual_camera/calibration_loader.h`
- Create: `src/calibration_loader.cpp`
- Test: `tests/test_calibration_loader.cpp`

- [ ] **Step 1: Write the failing test**

```cpp
#include "virtual_camera/calibration_loader.h"

#include <stdexcept>

namespace {

void Expect(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void TestLoadCalibrationFromRt024Dataset() {
  const vc::CalibrationParam calibration =
      vc::LoadRt024Calibration("/workspace/GACRT024_1754812994/calib_extract",
                               "calib_camera_front_wide_to_car.json",
                               "camera-front-wide",
                               "camera-front-wide-to-car",
                               0);
  Expect(calibration.dist_data.size() == 8, "pinhole distortion size");
  Expect(calibration.intrinsic_matrix(0, 0) > 0.0, "fx");
  Expect(calibration.extrinsic_matrix(3, 3) == 1.0, "homogeneous matrix");
}

void TestLoadCalibrationMissingKey() {
  bool thrown = false;
  try {
    static_cast<void>(vc::LoadRt024Calibration(
        "/workspace/GACRT024_1754812994/calib_extract",
        "calib_camera_front_wide_to_car.json",
        "missing-key", "camera-front-wide-to-car", 0));
  } catch (const std::runtime_error& error) {
    thrown = std::string(error.what()).find("missing-key") != std::string::npos;
  }
  Expect(thrown, "missing intri key should be reported");
}

}  // namespace

int main() {
  TestLoadCalibrationFromRt024Dataset();
  TestLoadCalibrationMissingKey();
  return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target test_calibration_loader`

Expected: FAIL because the new loader target and source files do not exist yet.

- [ ] **Step 3: Write minimal implementation**

```cpp
// include/virtual_camera/calibration_loader.h
#pragma once

#include "virtual_camera/types.h"

#include <string>

namespace vc {

CalibrationParam LoadRt024Calibration(const std::string& calib_dir,
                                      const std::string& conf_json,
                                      const std::string& intri_key,
                                      const std::string& extri_key,
                                      int distort_model);

}  // namespace vc
```

```cpp
// src/calibration_loader.cpp
#include "virtual_camera/calibration_loader.h"

#include "virtual_camera/json_utils.h"

#include <filesystem>

namespace vc {
namespace {

const nlohmann::json& RequiredNode(const nlohmann::json& json, const std::string& key) {
  if (!json.contains(key)) {
    throw std::runtime_error("missing calibration key: " + key);
  }
  return json.at(key);
}

}  // namespace

CalibrationParam LoadRt024Calibration(const std::string& calib_dir,
                                      const std::string& conf_json,
                                      const std::string& intri_key,
                                      const std::string& extri_key,
                                      int distort_model) {
  const std::filesystem::path path = std::filesystem::path(calib_dir) / conf_json;
  const nlohmann::json root = ReadJson(path.string());
  const auto& intri = RequiredNode(root, intri_key);
  const auto& extri = RequiredNode(root, extri_key);

  CalibrationParam calibration;
  calibration.intrinsic_matrix =
      JsonToMatrix3d(intri.at("param").at("cam_matrix").at("data"));
  calibration.extrinsic_matrix =
      JsonToMatrix4d(extri.at("param").at("sensor_calib").at("data"));
  calibration.dist_data =
      JsonToVector(intri.at("param").at("cam_dist").at("data"));

  const std::size_t expected = distort_model == 1 ? 4 : 8;
  if (calibration.dist_data.size() < expected) {
    throw std::runtime_error("distortion coeffs size too small in " + path.string());
  }
  calibration.dist_data.resize(expected);
  return calibration;
}

}  // namespace vc
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --target test_calibration_loader && ./build/test_calibration_loader`

Expected: PASS with exit code `0`.

- [ ] **Step 5: Commit**

```bash
git add include/virtual_camera/calibration_loader.h src/calibration_loader.cpp tests/test_calibration_loader.cpp CMakeLists.txt
git commit -m "feat: load RT024 calibration files"
```

## Task 3: Add RT024 JSON Writers

**Files:**
- Modify: `include/virtual_camera/json_writer.h`
- Modify: `src/json_writer.cpp`
- Test: `tests/test_rt024_json_writer.cpp`

- [ ] **Step 1: Write the failing test**

```cpp
#include "virtual_camera/json_writer.h"

#include <fstream>
#include <stdexcept>

namespace {

void Expect(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void TestWriteUndistortJson() {
  const std::string output = "build/test_tmp/rt024_json/undistort.json";
  vc::CalibrationParam calibration;
  calibration.extrinsic_matrix = Eigen::Matrix4d::Identity();
  calibration.intrinsic_matrix = Eigen::Matrix3d::Identity();
  calibration.dist_data = {0, 0, 0, 0, 0, 0, 0, 0};
  vc::NewIntrinsicConfig new_intrinsic;
  new_intrinsic.focal_u = 100.0;
  new_intrinsic.center_u = 50.0;
  new_intrinsic.focal_v = 100.0;
  new_intrinsic.center_v = 20.0;
  new_intrinsic.image_width = 128;
  new_intrinsic.image_height = 64;
  vc::WriteRt024UndistortJson(output, "camera-front-wide", calibration, new_intrinsic);
  const auto json = vc::ReadJson(output);
  Expect(json.contains("undistort_setting"), "undistort_setting");
}

void TestWriteVirtualJson() {
  const std::string output = "build/test_tmp/rt024_json/virtual.json";
  vc::CalibrationParam calibration;
  calibration.extrinsic_matrix = Eigen::Matrix4d::Identity();
  calibration.dist_data = {1, 2, 3, 4, 5, 6, 7, 8};
  Eigen::Matrix3d intrinsic = Eigen::Matrix3d::Identity();
  Eigen::Matrix4d extrinsic = Eigen::Matrix4d::Identity();
  vc::WriteRt024VirtualJson(output, "calib_camera_front_wide_to_car.json",
                            intrinsic, extrinsic, calibration.dist_data, 1024, 512);
  const auto json = vc::ReadJson(output);
  Expect(json.contains("value0"), "value0");
}

}  // namespace

int main() {
  TestWriteUndistortJson();
  TestWriteVirtualJson();
  return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target test_rt024_json_writer`

Expected: FAIL because the new writer functions are not declared or implemented.

- [ ] **Step 3: Write minimal implementation**

```cpp
// include/virtual_camera/json_writer.h
void WriteRt024UndistortJson(const std::string& output_path,
                             const std::string& sensor_name,
                             const CalibrationParam& calibration,
                             const NewIntrinsicConfig& new_intrinsic);

void WriteRt024VirtualJson(const std::string& output_path,
                           const std::string& sensor_name,
                           const Eigen::Matrix3d& virtual_intrinsic,
                           const Eigen::Matrix4d& virtual_extrinsic,
                           const std::vector<double>& dist_data,
                           int image_width,
                           int image_height);
```

```cpp
// src/json_writer.cpp
void WriteRt024UndistortJson(const std::string& output_path,
                             const std::string& sensor_name,
                             const CalibrationParam& calibration,
                             const NewIntrinsicConfig& new_intrinsic) {
  nlohmann::json json = {
      {"undistort_setting",
       {{"sensor_name", sensor_name},
        {"image_width", new_intrinsic.image_width},
        {"image_height", new_intrinsic.image_height},
        {"cam_matrix",
         {{"data",
           {{new_intrinsic.focal_u, 0.0, new_intrinsic.center_u},
            {0.0, new_intrinsic.focal_v, new_intrinsic.center_v},
            {0.0, 0.0, 1.0}}}}},
        {"sensor_calib", {{"data", Matrix4dToJson(calibration.extrinsic_matrix)}}}}}};
  WriteJson(output_path, json);
}

void WriteRt024VirtualJson(const std::string& output_path,
                           const std::string& sensor_name,
                           const Eigen::Matrix3d& virtual_intrinsic,
                           const Eigen::Matrix4d& virtual_extrinsic,
                           const std::vector<double>& dist_data,
                           int image_width,
                           int image_height) {
  nlohmann::json json = {
      {"value0",
       {{"sensor_name", sensor_name},
        {"target_sensor_name", sensor_name},
        {"param",
         {{"img_new_w", image_width},
          {"img_new_h", image_height},
          {"cam_K_new", {{"data", Matrix3dToJson(virtual_intrinsic)}}},
          {"cam_dist", {{"cols", dist_data.size()}, {"data", {dist_data}}}},
          {"sensor_calib", {{"data", Matrix4dToJson(virtual_extrinsic)}}}}}}}};
  WriteJson(output_path, json);
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --target test_rt024_json_writer && ./build/test_rt024_json_writer`

Expected: PASS with exit code `0`.

- [ ] **Step 5: Commit**

```bash
git add include/virtual_camera/json_writer.h src/json_writer.cpp tests/test_rt024_json_writer.cpp CMakeLists.txt
git commit -m "feat: add RT024 calibration json writers"
```

## Task 4: Add RT024 Output Verification

**Files:**
- Modify: `include/virtual_camera/verifier.h`
- Modify: `src/verifier.cpp`
- Test: `tests/test_rt024_verifier.cpp`

- [ ] **Step 1: Write the failing test**

```cpp
#include "virtual_camera/verifier.h"

#include <fstream>
#include <stdexcept>

namespace {

void WriteText(const std::string& path, const std::string& content) {
  std::ofstream output(path);
  output << content;
}

void Expect(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void TestVerifyRt024OutputsPass() {
  const std::string golden = "build/test_tmp/rt024_verify/golden";
  const std::string actual = "build/test_tmp/rt024_verify/actual";
  std::filesystem::create_directories(golden + "/vc_gdcbin_dir_path");
  std::filesystem::create_directories(actual + "/vc_gdcbin_dir_path");
  std::filesystem::create_directories(golden + "/calib_virtual_camera");
  std::filesystem::create_directories(actual + "/calib_virtual_camera");
  std::filesystem::create_directories(golden + "/calib_undistortion");
  std::filesystem::create_directories(actual + "/calib_undistortion");
  WriteText(golden + "/vc_gdcbin_dir_path/a.bin", "abc");
  WriteText(actual + "/vc_gdcbin_dir_path/a.bin", "abc");
  WriteText(golden + "/calib_virtual_camera/a.json", "{\"value0\":1}");
  WriteText(actual + "/calib_virtual_camera/a.json", "{\"value0\":1}");
  WriteText(golden + "/calib_undistortion/a.json", "{\"undistort_setting\":1}");
  WriteText(actual + "/calib_undistortion/a.json", "{\"undistort_setting\":1}");
  const auto result = vc::VerifyRt024Outputs(golden, actual);
  Expect(result.ok, result.message);
}

void TestVerifyRt024OutputsFailOnBinDiff() {
  const std::string golden = "build/test_tmp/rt024_verify_diff/golden";
  const std::string actual = "build/test_tmp/rt024_verify_diff/actual";
  std::filesystem::create_directories(golden + "/vc_gdcbin_dir_path");
  std::filesystem::create_directories(actual + "/vc_gdcbin_dir_path");
  std::filesystem::create_directories(golden + "/calib_virtual_camera");
  std::filesystem::create_directories(actual + "/calib_virtual_camera");
  std::filesystem::create_directories(golden + "/calib_undistortion");
  std::filesystem::create_directories(actual + "/calib_undistortion");
  WriteText(golden + "/vc_gdcbin_dir_path/a.bin", "abc");
  WriteText(actual + "/vc_gdcbin_dir_path/a.bin", "abd");
  WriteText(golden + "/calib_virtual_camera/a.json", "{\"value0\":1}");
  WriteText(actual + "/calib_virtual_camera/a.json", "{\"value0\":1}");
  WriteText(golden + "/calib_undistortion/a.json", "{\"undistort_setting\":1}");
  WriteText(actual + "/calib_undistortion/a.json", "{\"undistort_setting\":1}");
  const auto result = vc::VerifyRt024Outputs(golden, actual);
  Expect(!result.ok, "expected diff");
}

}  // namespace

int main() {
  TestVerifyRt024OutputsPass();
  TestVerifyRt024OutputsFailOnBinDiff();
  return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target test_rt024_verifier`

Expected: FAIL because `VerifyRt024Outputs` does not exist yet.

- [ ] **Step 3: Write minimal implementation**

```cpp
// include/virtual_camera/verifier.h
VerifyResult VerifyRt024Outputs(const std::string& golden_root,
                                const std::string& actual_root);
```

```cpp
// src/verifier.cpp
VerifyResult VerifyRt024Outputs(const std::string& golden_root,
                                const std::string& actual_root) {
  const auto bin_result = CompareDirectoryFiles(golden_root + "/vc_gdcbin_dir_path",
                                                actual_root + "/vc_gdcbin_dir_path",
                                                ".bin");
  if (!bin_result.ok) {
    return bin_result;
  }
  const auto virtual_result = CompareDirectoryFiles(golden_root + "/calib_virtual_camera",
                                                    actual_root + "/calib_virtual_camera",
                                                    ".json");
  if (!virtual_result.ok) {
    return virtual_result;
  }
  const auto undistort_result = CompareDirectoryFiles(
      golden_root + "/calib_undistortion",
      actual_root + "/calib_undistortion",
      ".json");
  if (!undistort_result.ok) {
    return undistort_result;
  }
  return {true, "verification passed"};
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --target test_rt024_verifier && ./build/test_rt024_verifier`

Expected: PASS with exit code `0`.

- [ ] **Step 5: Commit**

```bash
git add include/virtual_camera/verifier.h src/verifier.cpp tests/test_rt024_verifier.cpp CMakeLists.txt
git commit -m "feat: add RT024 output verification"
```

## Task 5: Implement Undistort Processor

**Files:**
- Create: `include/virtual_camera/undistort_processor.h`
- Create: `src/undistort_processor.cpp`
- Modify: `src/json_writer.cpp`
- Test: `tests/test_rt024_json_writer.cpp`

- [ ] **Step 1: Write the failing test**

```cpp
void TestWriteUndistortJsonUsesZeroDistortion() {
  const std::string output = "build/test_tmp/rt024_json/undistort_zero.json";
  vc::CalibrationParam calibration;
  calibration.extrinsic_matrix = Eigen::Matrix4d::Identity();
  calibration.dist_data = {9, 9, 9, 9, 9, 9, 9, 9};
  vc::NewIntrinsicConfig new_intrinsic;
  new_intrinsic.focal_u = 100.0;
  new_intrinsic.center_u = 50.0;
  new_intrinsic.focal_v = 100.0;
  new_intrinsic.center_v = 20.0;
  new_intrinsic.image_width = 128;
  new_intrinsic.image_height = 64;
  vc::WriteRt024UndistortJson(output, "camera-front-wide", calibration, new_intrinsic);
  const auto json = vc::ReadJson(output);
  const auto dist = json.at("undistort_setting").at("cam_dist").at("data");
  Expect(dist.at(0).get<double>() == 0.0, "undistort distortion should be zero");
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target test_rt024_json_writer && ./build/test_rt024_json_writer`

Expected: FAIL because the undistort JSON currently does not contain zeroed `cam_dist`.

- [ ] **Step 3: Write minimal implementation**

```cpp
// src/json_writer.cpp
void WriteRt024UndistortJson(const std::string& output_path,
                             const std::string& sensor_name,
                             const CalibrationParam& calibration,
                             const NewIntrinsicConfig& new_intrinsic) {
  std::vector<double> zero_dist(calibration.dist_data.size(), 0.0);
  nlohmann::json json = {
      {"undistort_setting",
       {{"sensor_name", sensor_name},
        {"image_width", new_intrinsic.image_width},
        {"image_height", new_intrinsic.image_height},
        {"cam_matrix",
         {{"data",
           {{new_intrinsic.focal_u, 0.0, new_intrinsic.center_u},
            {0.0, new_intrinsic.focal_v, new_intrinsic.center_v},
            {0.0, 0.0, 1.0}}}}},
        {"cam_dist", {{"data", zero_dist}}},
        {"sensor_calib", {{"data", Matrix4dToJson(calibration.extrinsic_matrix)}}}}}};
  WriteJson(output_path, json);
}
```

```cpp
// include/virtual_camera/undistort_processor.h
#pragma once

#include "virtual_camera/pipeline_config.h"

namespace vc {

void RunUndistortPipeline(const PipelineConfig& config, const std::string& output_root);

}  // namespace vc
```

```cpp
// src/undistort_processor.cpp
#include "virtual_camera/undistort_processor.h"

#include "virtual_camera/calibration_loader.h"
#include "virtual_camera/json_writer.h"
#include "virtual_camera/json_utils.h"
#include "virtual_camera/map_generator.h"

#include <filesystem>
#include <opencv2/opencv.hpp>

namespace vc {
namespace {

std::filesystem::path Join(const std::string& root, const std::string& child) {
  return std::filesystem::path(root) / child;
}

}  // namespace

void RunUndistortPipeline(const PipelineConfig& config, const std::string& output_root) {
  for (const auto& task : config.undistort_tasks) {
    const CalibrationParam calibration = LoadRt024Calibration(
        Join(config.dataset_root, config.paths.conf_dir_path).string(),
        task.conf_json, task.intri_key, task.extri_key, config.distort_model);
    MapGenerator generator(calibration, VirtualParam{});
    const auto maps = generator.CreateUndistortMap();

    const std::filesystem::path json_output =
        Join(output_root, config.paths.undistort_conf_dir_path) / task.conf_json;
    WriteRt024UndistortJson(json_output.string(), task.intri_key, calibration,
                            task.new_intrinsic);

    const std::filesystem::path image_input =
        Join(config.dataset_root, config.paths.image_dir_path) / task.image_dir;
    const std::filesystem::path image_output =
        Join(output_root, config.paths.undistort_image_dir_path) / task.image_dir;
    EnsureDirectory(image_output.string());
    for (const auto& entry : std::filesystem::directory_iterator(image_input)) {
      if (!entry.is_regular_file()) {
        continue;
      }
      const cv::Mat image = cv::imread(entry.path().string(), cv::IMREAD_COLOR);
      cv::Mat remapped;
      cv::remap(image, remapped, maps.map_x, maps.map_y, cv::INTER_LINEAR);
      cv::imwrite((image_output / entry.path().filename()).string(), remapped);
    }
  }
}

}  // namespace vc
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --target test_rt024_json_writer && ./build/test_rt024_json_writer`

Expected: PASS with exit code `0`.

- [ ] **Step 5: Commit**

```bash
git add include/virtual_camera/undistort_processor.h src/undistort_processor.cpp src/json_writer.cpp
git commit -m "feat: add RT024 undistort processing"
```

## Task 6: Implement Virtual Camera Processor

**Files:**
- Create: `include/virtual_camera/virtual_camera_processor.h`
- Create: `src/virtual_camera_processor.cpp`
- Modify: `src/json_writer.cpp`
- Test: `tests/test_rt024_json_writer.cpp`

- [ ] **Step 1: Write the failing test**

```cpp
void TestWriteVirtualJsonCarriesImageSize() {
  const std::string output = "build/test_tmp/rt024_json/virtual_size.json";
  Eigen::Matrix3d intrinsic = Eigen::Matrix3d::Identity();
  Eigen::Matrix4d extrinsic = Eigen::Matrix4d::Identity();
  vc::WriteRt024VirtualJson(output, "sensor", intrinsic, extrinsic,
                            std::vector<double>{1, 2, 3, 4}, 1024, 512);
  const auto json = vc::ReadJson(output);
  Expect(json.at("value0").at("param").at("img_new_w").get<int>() == 1024, "img_new_w");
  Expect(json.at("value0").at("param").at("img_new_h").get<int>() == 512, "img_new_h");
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target test_rt024_json_writer && ./build/test_rt024_json_writer`

Expected: FAIL because the virtual JSON layout is incomplete or missing expected fields.

- [ ] **Step 3: Write minimal implementation**

```cpp
// src/json_writer.cpp
void WriteRt024VirtualJson(const std::string& output_path,
                           const std::string& sensor_name,
                           const Eigen::Matrix3d& virtual_intrinsic,
                           const Eigen::Matrix4d& virtual_extrinsic,
                           const std::vector<double>& dist_data,
                           int image_width,
                           int image_height) {
  nlohmann::json json = {
      {"value0",
       {{"sensor_name", sensor_name},
        {"target_sensor_name", sensor_name},
        {"param",
         {{"img_new_w", image_width},
          {"img_new_h", image_height},
          {"cam_K_new", {{"data", Matrix3dToJson(virtual_intrinsic)}}},
          {"cam_dist", {{"cols", dist_data.size()}, {"data", {dist_data}}}},
          {"sensor_calib", {{"data", Matrix4dToJson(virtual_extrinsic)}}}}}}}};
  WriteJson(output_path, json);
}
```

```cpp
// include/virtual_camera/virtual_camera_processor.h
#pragma once

#include "virtual_camera/pipeline_config.h"

namespace vc {

void RunVirtualCameraPipeline(const PipelineConfig& config, const std::string& output_root);

}  // namespace vc
```

```cpp
// src/virtual_camera_processor.cpp
#include "virtual_camera/virtual_camera_processor.h"

#include "virtual_camera/calibration_loader.h"
#include "virtual_camera/json_writer.h"
#include "virtual_camera/json_utils.h"
#include "virtual_camera/map_generator.h"

#include <filesystem>
#include <opencv2/opencv.hpp>

namespace vc {
namespace {

VirtualParam ToVirtualParam(const VirtualCameraTaskConfig& task) {
  VirtualParam param;
  param.virtual_width = task.new_intrinsic.image_width;
  param.virtual_height = task.new_intrinsic.image_height;
  param.virtual_fov = task.new_intrinsic.fov;
  param.virtual_fu = task.new_intrinsic.focal_u;
  param.virtual_cx = task.new_intrinsic.center_u;
  param.virtual_fv = task.new_intrinsic.focal_v;
  param.virtual_cy = task.new_intrinsic.center_v;
  param.virtual_pitch = task.new_extrinsics.pitch;
  param.virtual_roll = task.new_extrinsics.roll;
  param.virtual_yaw = task.new_extrinsics.yaw;
  param.tx = task.new_extrinsics.x;
  param.ty = task.new_extrinsics.y;
  param.tz = task.new_extrinsics.z;
  param.image_width = task.image_width;
  param.image_height = task.image_height;
  param.fov = task.fov;
  param.camera_id = task.camera_id;
  return param;
}

}  // namespace

void RunVirtualCameraPipeline(const PipelineConfig& config, const std::string& output_root) {
  for (const auto& task : config.virtual_tasks) {
    const CalibrationParam calibration = LoadRt024Calibration(
        (std::filesystem::path(config.dataset_root) / config.paths.conf_dir_path).string(),
        task.conf_json, task.conf_intri_key, task.conf_extri_key, config.distort_model);
    MapGenerator generator(calibration, ToVirtualParam(task));
    const auto maps = generator.CreateVirtualMap(true);

    const std::filesystem::path bin_output =
        std::filesystem::path(output_root) / config.paths.vc_gdcbin_dir_path;
    EnsureDirectory(bin_output.string());
    generator.SaveSingleMap((bin_output / task.vc_mapx_name).string(), maps.map_x);
    generator.SaveSingleMap((bin_output / task.vc_mapy_name).string(), maps.map_y);
    generator.SaveSingleMap((bin_output / task.src2vc_mapx_name).string(), maps.src2dst_mapx);
    generator.SaveSingleMap((bin_output / task.src2vc_mapy_name).string(), maps.src2dst_mapy);

    const std::filesystem::path json_output =
        std::filesystem::path(output_root) / config.paths.vc_conf_dir_path / task.conf_json;
    WriteRt024VirtualJson(json_output.string(), task.conf_json,
                          generator.virtual_intrinsic(), generator.virtual_extrinsic(),
                          calibration.dist_data, task.new_intrinsic.image_width,
                          task.new_intrinsic.image_height);

    const std::filesystem::path image_input =
        std::filesystem::path(config.dataset_root) / config.paths.image_dir_path / task.image_dir;
    const std::filesystem::path image_output =
        std::filesystem::path(output_root) / config.paths.vc_image_dir_path / task.save_dir;
    EnsureDirectory(image_output.string());
    for (const auto& entry : std::filesystem::directory_iterator(image_input)) {
      if (!entry.is_regular_file()) {
        continue;
      }
      const cv::Mat image = cv::imread(entry.path().string(), cv::IMREAD_COLOR);
      cv::Mat remapped;
      cv::remap(image, remapped, maps.map_x, maps.map_y, cv::INTER_LINEAR);
      cv::imwrite((image_output / entry.path().filename()).string(), remapped);
    }
  }
}

}  // namespace vc
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --target test_rt024_json_writer && ./build/test_rt024_json_writer`

Expected: PASS with exit code `0`.

- [ ] **Step 5: Commit**

```bash
git add include/virtual_camera/virtual_camera_processor.h src/virtual_camera_processor.cpp src/json_writer.cpp
git commit -m "feat: add RT024 virtual camera processing"
```

## Task 7: Replace Main Entry And Wire Full Pipeline

**Files:**
- Modify: `src/main.cpp`
- Modify: `CMakeLists.txt`
- Create: `configs/config_rt024.json`

- [ ] **Step 1: Write the failing test**

```bash
cmake --build build --target virtual_camera_tool
./build/virtual_camera_tool
```

Expected: FAIL with old usage text because the single-config entry has not been implemented yet.

- [ ] **Step 2: Run test to verify it fails**

Run: `./build/virtual_camera_tool`

Expected: current binary exits non-zero and prints old `generate-verify` / `generate-4v` usage.

- [ ] **Step 3: Write minimal implementation**

```cpp
// src/main.cpp
#include "virtual_camera/pipeline_config.h"
#include "virtual_camera/undistort_processor.h"
#include "virtual_camera/virtual_camera_processor.h"
#include "virtual_camera/verifier.h"

#include <filesystem>
#include <iostream>

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <config.json>\n";
    return 1;
  }

  try {
    const vc::PipelineConfig config = vc::LoadPipelineConfig(argv[1]);
    const std::string output_root = config.dataset_root;
    if (config.process_undistort) {
      vc::RunUndistortPipeline(config, output_root);
    }
    if (config.process_virtual_camera) {
      vc::RunVirtualCameraPipeline(config, output_root);
    }
    const vc::VerifyResult result =
        vc::VerifyRt024Outputs(config.dataset_root, output_root);
    if (!result.ok) {
      std::cerr << result.message << "\n";
      return 2;
    }
    std::cout << result.message << "\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << "\n";
    return 1;
  }
}
```

```json
// configs/config_rt024.json
{
  "dataset_root": "/workspace/GACRT024_1754812994",
  "conf_dir_path": "calib_extract/",
  "image_dir_path": "image_raw/",
  "vc_image_dir_path": "image_virtual_camera/",
  "undistort_image_dir_path": "image_undistortion/",
  "vc_conf_dir_path": "calib_virtual_camera/",
  "undistort_conf_dir_path": "calib_undistortion/",
  "vc_gdcbin_dir_path": "vc_gdcbin_dir_path/",
  "showinfo": 1,
  "showdir": 0,
  "process_virtual_camera": 1,
  "process_undistort": 1,
  "undistort_image": 0,
  "distort_model": 0,
  "save_virtual_json": 1,
  "save_undistort_json": 1,
  "conf_type": 3,
  "virtual_camera_configs": [],
  "undistort_configs": []
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --target virtual_camera_tool && ./build/virtual_camera_tool configs/config_rt024.json`

Expected: program starts the new pipeline, then either completes or fails only on missing task content inside `config_rt024.json`.

- [ ] **Step 5: Commit**

```bash
git add src/main.cpp configs/config_rt024.json CMakeLists.txt
git commit -m "feat: replace CLI with RT024 config entry"
```

## Task 8: Fill RT024 Config And Run Integration Verification

**Files:**
- Modify: `configs/config_rt024.json`
- Modify: `README.md`

- [ ] **Step 1: Write the failing test**

```bash
./build/virtual_camera_tool configs/config_rt024.json
```

Expected: FAIL because the config does not yet contain the real RT024 task list.

- [ ] **Step 2: Run test to verify it fails**

Run: `./build/virtual_camera_tool configs/config_rt024.json`

Expected: non-zero exit caused by empty or incomplete task arrays.

- [ ] **Step 3: Write minimal implementation**

```json
{
  "dataset_root": "/workspace/GACRT024_1754812994",
  "conf_dir_path": "calib_extract/",
  "image_dir_path": "image_raw/",
  "vc_image_dir_path": "image_virtual_camera/",
  "undistort_image_dir_path": "image_undistortion/",
  "vc_conf_dir_path": "calib_virtual_camera/",
  "undistort_conf_dir_path": "calib_undistortion/",
  "vc_gdcbin_dir_path": "vc_gdcbin_dir_path/",
  "showinfo": 1,
  "showdir": 0,
  "process_virtual_camera": 1,
  "process_undistort": 1,
  "undistort_image": 0,
  "distort_model": 0,
  "save_virtual_json": 1,
  "save_undistort_json": 1,
  "conf_type": 3,
  "virtual_camera_configs": [
    {
      "desc": "front narrow -> front narrow 30",
      "conf_json": "calib_camera_front_narrow_to_car.json",
      "conf_intri_key": "camera-front-narrow",
      "conf_extri_key": "camera-front-narrow-to-car",
      "image_dir": "front_narrow/",
      "save_dir": "front_narrow_30/",
      "camera_id": 0,
      "image_width": 3840,
      "image_height": 2160,
      "fov": 30,
      "undistort_image": 0,
      "vc_mapX_name": "ft30_vc_mapX.bin",
      "vc_mapY_name": "ft30_vc_mapY.bin",
      "src2vc_mapX_name": "ft30_src2vc_mapX.bin",
      "src2vc_mapY_name": "ft30_src2vc_mapY.bin"
    },
    {
      "desc": "front wide -> front wide 110",
      "conf_json": "calib_camera_front_wide_to_car.json",
      "conf_intri_key": "camera-front-wide",
      "conf_extri_key": "camera-front-wide-to-car",
      "image_dir": "front_wide/",
      "save_dir": "front_wide_110/",
      "camera_id": 1,
      "image_width": 3840,
      "image_height": 2160,
      "fov": 120,
      "undistort_image": 1,
      "vc_mapX_name": "fw110_vc_mapX.bin",
      "vc_mapY_name": "fw110_vc_mapY.bin",
      "src2vc_mapX_name": "fw110_src2vc_mapX.bin",
      "src2vc_mapY_name": "fw110_src2vc_mapY.bin"
    },
    {
      "desc": "left front -> left front 99",
      "conf_json": "calib_camera_left_front_to_car.json",
      "conf_intri_key": "camera-left-front",
      "conf_extri_key": "camera-left-front-to-car",
      "image_dir": "left_front/",
      "save_dir": "left_front_99/",
      "camera_id": 2,
      "image_width": 1920,
      "image_height": 1280,
      "fov": 100,
      "undistort_image": 1,
      "vc_mapX_name": "fl99_vc_mapX.bin",
      "vc_mapY_name": "fl99_vc_mapY.bin",
      "src2vc_mapX_name": "fl99_src2vc_mapX.bin",
      "src2vc_mapY_name": "fl99_src2vc_mapY.bin"
    },
    {
      "desc": "left back -> left back 99",
      "conf_json": "calib_camera_left_back_to_car.json",
      "conf_intri_key": "camera-left-back",
      "conf_extri_key": "camera-left-back-to-car",
      "image_dir": "left_back/",
      "save_dir": "left_back_99/",
      "camera_id": 3,
      "image_width": 1920,
      "image_height": 1280,
      "fov": 100,
      "undistort_image": 1,
      "vc_mapX_name": "rl99_vc_mapX.bin",
      "vc_mapY_name": "rl99_vc_mapY.bin",
      "src2vc_mapX_name": "rl99_src2vc_mapX.bin",
      "src2vc_mapY_name": "rl99_src2vc_mapY.bin"
    },
    {
      "desc": "right front -> right front 99",
      "conf_json": "calib_camera_right_front_to_car.json",
      "conf_intri_key": "camera-right-front",
      "conf_extri_key": "camera-right-front-to-car",
      "image_dir": "right_front/",
      "save_dir": "right_front_99/",
      "camera_id": 4,
      "image_width": 1920,
      "image_height": 1280,
      "fov": 100,
      "undistort_image": 1,
      "vc_mapX_name": "fr99_vc_mapX.bin",
      "vc_mapY_name": "fr99_vc_mapY.bin",
      "src2vc_mapX_name": "fr99_src2vc_mapX.bin",
      "src2vc_mapY_name": "fr99_src2vc_mapY.bin"
    },
    {
      "desc": "right back -> right back 99",
      "conf_json": "calib_camera_right_back_to_car.json",
      "conf_intri_key": "camera-right-back",
      "conf_extri_key": "camera-right-back-to-car",
      "image_dir": "right_back/",
      "save_dir": "right_back_99/",
      "camera_id": 5,
      "image_width": 1920,
      "image_height": 1280,
      "fov": 100,
      "undistort_image": 1,
      "vc_mapX_name": "rr99_vc_mapX.bin",
      "vc_mapY_name": "rr99_vc_mapY.bin",
      "src2vc_mapX_name": "rr99_src2vc_mapX.bin",
      "src2vc_mapY_name": "rr99_src2vc_mapY.bin"
    },
    {
      "desc": "back -> back 50",
      "conf_json": "calib_camera_back_to_car.json",
      "conf_intri_key": "camera-back",
      "conf_extri_key": "camera-back-to-car",
      "image_dir": "back/",
      "save_dir": "back_50/",
      "camera_id": 6,
      "image_width": 1920,
      "image_height": 1280,
      "fov": 100,
      "undistort_image": 1,
      "vc_mapX_name": "r50_vc_mapX.bin",
      "vc_mapY_name": "r50_vc_mapY.bin",
      "src2vc_mapX_name": "r50_src2vc_mapX.bin",
      "src2vc_mapY_name": "r50_src2vc_mapY.bin"
    },
    {
      "desc": "front wide -> front wide 60",
      "conf_json": "calib_camera_front_wide_to_car.json",
      "conf_intri_key": "camera-front-wide",
      "conf_extri_key": "camera-front-wide-to-car",
      "image_dir": "front_wide/",
      "save_dir": "front_wide_60/",
      "camera_id": 1,
      "image_width": 3840,
      "image_height": 2160,
      "fov": 120,
      "undistort_image": 1,
      "vc_mapX_name": "fw60_vc_mapX.bin",
      "vc_mapY_name": "fw60_vc_mapY.bin",
      "src2vc_mapX_name": "fw60_src2vc_mapX.bin",
      "src2vc_mapY_name": "fw60_src2vc_mapY.bin"
    },
    {
      "desc": "front narrow -> front narrow 20",
      "conf_json": "calib_camera_front_narrow_to_car.json",
      "conf_intri_key": "camera-front-narrow",
      "conf_extri_key": "camera-front-narrow-to-car",
      "image_dir": "front_narrow/",
      "save_dir": "front_narrow_20/",
      "camera_id": 0,
      "image_width": 3840,
      "image_height": 2160,
      "fov": 30,
      "undistort_image": 0,
      "vc_mapX_name": "ft20_vc_mapX.bin",
      "vc_mapY_name": "ft20_vc_mapY.bin",
      "src2vc_mapX_name": "ft20_src2vc_mapX.bin",
      "src2vc_mapY_name": "ft20_src2vc_mapY.bin"
    },
    {
      "desc": "left back -> left back 30",
      "conf_json": "calib_camera_left_back_to_car.json",
      "conf_intri_key": "camera-left-back",
      "conf_extri_key": "camera-left-back-to-car",
      "image_dir": "left_back/",
      "save_dir": "left_back_30/",
      "camera_id": 3,
      "image_width": 1920,
      "image_height": 1280,
      "fov": 100,
      "undistort_image": 1,
      "vc_mapX_name": "rl30_vc_mapX.bin",
      "vc_mapY_name": "rl30_vc_mapY.bin",
      "src2vc_mapX_name": "rl30_src2vc_mapX.bin",
      "src2vc_mapY_name": "rl30_src2vc_mapY.bin"
    },
    {
      "desc": "right back -> right back 30",
      "conf_json": "calib_camera_right_back_to_car.json",
      "conf_intri_key": "camera-right-back",
      "conf_extri_key": "camera-right-back-to-car",
      "image_dir": "right_back/",
      "save_dir": "right_back_30/",
      "camera_id": 5,
      "image_width": 1920,
      "image_height": 1280,
      "fov": 100,
      "undistort_image": 1,
      "vc_mapX_name": "rr30_vc_mapX.bin",
      "vc_mapY_name": "rr30_vc_mapY.bin",
      "src2vc_mapX_name": "rr30_src2vc_mapX.bin",
      "src2vc_mapY_name": "rr30_src2vc_mapY.bin"
    },
    {
      "desc": "front wide -> front wide 30",
      "conf_json": "calib_camera_front_wide_to_car.json",
      "conf_intri_key": "camera-front-wide",
      "conf_extri_key": "camera-front-wide-to-car",
      "image_dir": "front_wide/",
      "save_dir": "front_wide_30/",
      "camera_id": 1,
      "image_width": 3840,
      "image_height": 2160,
      "fov": 120,
      "undistort_image": 1,
      "vc_mapX_name": "fw30_vc_mapX.bin",
      "vc_mapY_name": "fw30_vc_mapY.bin",
      "src2vc_mapX_name": "fw30_src2vc_mapX.bin",
      "src2vc_mapY_name": "fw30_src2vc_mapY.bin"
    }
  ],
  "undistort_configs": [
    {
      "conf_json": "calib_camera_front_narrow_to_car.json",
      "intri_key": "camera-front-narrow",
      "extri_key": "camera-front-narrow-to-car",
      "image_dir": "front_narrow/"
    },
    {
      "conf_json": "calib_camera_front_wide_to_car.json",
      "intri_key": "camera-front-wide",
      "extri_key": "camera-front-wide-to-car",
      "image_dir": "front_wide/"
    },
    {
      "conf_json": "calib_camera_left_front_to_car.json",
      "intri_key": "camera-left-front",
      "extri_key": "camera-left-front-to-car",
      "image_dir": "left_front/"
    },
    {
      "conf_json": "calib_camera_left_back_to_car.json",
      "intri_key": "camera-left-back",
      "extri_key": "camera-left-back-to-car",
      "image_dir": "left_back/"
    },
    {
      "conf_json": "calib_camera_right_front_to_car.json",
      "intri_key": "camera-right-front",
      "extri_key": "camera-right-front-to-car",
      "image_dir": "right_front/"
    },
    {
      "conf_json": "calib_camera_right_back_to_car.json",
      "intri_key": "camera-right-back",
      "extri_key": "camera-right-back-to-car",
      "image_dir": "right_back/"
    },
    {
      "conf_json": "calib_camera_back_to_car.json",
      "intri_key": "camera-back",
      "extri_key": "camera-back-to-car",
      "image_dir": "back/"
    }
  ]
}
```

为避免手工抄错，`configs/config_rt024.json` 中各任务的 `new_intrinsic` / `new_extrinsics` 数值按下面的固定来源填写：

```bash
sed -n '43,492p' /workspace/gen_vc_bin_lib_test/config.json
sed -n '2095,2248p' /workspace/gen_vc_bin_lib_test/config.json
```

对应关系固定为：

- `front_narrow_30` -> `front fov30->front fov 30`
- `front_wide_110` -> `front fov120->front fov 110`
- `left_front_99` -> `front left fov100->front left fov 99`
- `left_back_99` -> `rear left fov100->rear left fov 99`
- `right_front_99` -> `front right fov 100->front right fov 99`
- `right_back_99` -> `rear right fov 100->rear right fov 99`
- `back_50` -> `rear fov 60->rear fov 50`
- `front_wide_60` -> `front fov120->front fov 60`
- `front_narrow_20` -> `front fov30->front fov 20`
- `left_back_30` -> `rear left fov100->rear left fov 30`
- `right_back_30` -> `rear right fov 100->rear right fov 30`
- `front_wide_30` -> 以 `calib_virtual_camera/calib_cam_front_wide_fov30.json` 与 `image_virtual_camera/front_wide_30/` 为真值补齐，同名 map 文件为 `fw30_*`

去畸变任务的 `new_intrinsic` 数值按 `/workspace/gen_vc_bin_lib_test/config.json` 中 `undistort_configs` 段逐项填写，目录与相机名一一对应：

- `front_narrow/` -> `camera-front-narrow`
- `front_wide/` -> `camera-front-wide`
- `left_front/` -> `camera-left-front`
- `left_back/` -> `camera-left-back`
- `right_front/` -> `camera-right-front`
- `right_back/` -> `camera-right-back`
- `back/` -> `camera-back`

```md
## RT024 Run

```bash
bash scripts/build.sh
./build/virtual_camera_tool configs/config_rt024.json
```

输入数据固定为 `/workspace/GACRT024_1754812994`。当前自动校验包括：

- `vc_gdcbin_dir_path`
- `calib_virtual_camera`
- `calib_undistortion`

图像会输出到：

- `image_virtual_camera`
- `image_undistortion`
```

- [ ] **Step 4: Run test to verify it passes**

Run:

```bash
cmake --build build
./build/test_pipeline_config
./build/test_calibration_loader
./build/test_rt024_json_writer
./build/test_rt024_verifier
./build/virtual_camera_tool configs/config_rt024.json
```

Expected:

- 4 个测试目标全部 PASS
- 主程序返回 `0`
- 输出 `verification passed`
- `image_virtual_camera` 与 `image_undistortion` 目录包含生成后的图像文件

- [ ] **Step 5: Commit**

```bash
git add configs/config_rt024.json README.md
git commit -m "feat: wire RT024 dataset pipeline end to end"
```

## Self-Review

- 规格覆盖检查：
  - 新配置入口：Task 1, Task 7, Task 8
  - 标定读取：Task 2
  - 去畸变处理与图像输出：Task 5
- 虚拟相机处理、bin 输出与图像输出：Task 6
  - 参数 JSON 输出：Task 3, Task 5, Task 6
  - bin/json 校验：Task 4, Task 8
  - README 更新：Task 8
- 占位检查：
  - Task 8 未保留占位词；数值来源已固定到参考配置的明确区间和任务名映射。
- 类型一致性检查：
  - `PipelineConfig`、`VirtualCameraTaskConfig`、`UndistortTaskConfig`、`NewIntrinsicConfig` 在各任务中命名一致。
