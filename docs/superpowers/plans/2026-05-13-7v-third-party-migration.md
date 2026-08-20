# 7V Third-Party Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 移除 7v RT024 流程对工作区外 map 库和 4v 代码的依赖，只保留 vendored 的 `opencv`、`eigen`、`nlohmann_json`，并让 7v 的 bin 与参数输出通过固定数据集真值校验。

**Architecture:** 保留当前单入口 RT024 主流程和现有配置/校验模块，在仓内新增 `remap_generator` 封装去畸变与虚拟相机 map 生成，替换 `<gen_vc_map.hpp>` 的调用。同时清理 4v 代码、测试、配置和构建入口，使仓库只保留 7v 路径。

**Tech Stack:** C++17, CMake, OpenCV, Eigen, nlohmann_json, yaml-cpp

---

### Task 1: Vendoring 通用依赖并清理构建入口

**Files:**
- Create: `third_party/eigen/`
- Create: `third_party/opencv/`
- Modify: `third_party/nlohmann_json/include/`
- Modify: `CMakeLists.txt`
- Modify: `scripts/build.sh`
- Test: `build/compile_commands.json`

- [ ] **Step 1: 复制 `eigen` 和 `opencv` 到 `third_party`**

```bash
mkdir -p third_party
cp -a /workspace/icv_vc_bin_lib/deps/x86/eigen3-3.3.9 third_party/eigen
cp -a /workspace/icv_vc_bin_lib/deps/x86/opencv third_party/opencv
cp -a /workspace/gen_vc_bin_lib_test/deps/x86/nlohmann_json/include/. third_party/nlohmann_json/include/
```

Expected: `third_party/eigen/include/eigen3/Eigen`, `third_party/opencv/include`, `third_party/opencv/lib` 存在。

- [ ] **Step 2: 更新 `CMakeLists.txt` 的三方路径定义**

```cmake
set(THIRD_PARTY_ROOT ${PROJECT_SOURCE_DIR}/third_party)
set(VENDORED_OPENCV_ROOT ${THIRD_PARTY_ROOT}/opencv)
set(VENDORED_EIGEN_ROOT ${THIRD_PARTY_ROOT}/eigen)
set(VENDORED_JSON_ROOT ${THIRD_PARTY_ROOT}/nlohmann_json)
```

同时删除以下旧定义：

```cmake
set(REF_DEPS_ROOT /workspace/icv_vc_bin_lib/deps/x86)
set(REF_OPENCV_ROOT ${REF_DEPS_ROOT}/opencv)
set(REF_EIGEN_ROOT ${REF_DEPS_ROOT}/eigen3-3.3.9)
set(RT024_MAP_LIB_ROOT /workspace/gen_vc_bin_lib_test/deps/x86/gen_vc_map_lib)
```

- [ ] **Step 3: 更新 include 路径，只保留仓内依赖**

```cmake
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${VENDORED_JSON_ROOT}/include)
include_directories(${VENDORED_EIGEN_ROOT}/include/eigen3)
include_directories(${VENDORED_OPENCV_ROOT}/include)
```

删除：

```cmake
include_directories(${PROJECT_SOURCE_DIR}/third_party/gdc_add_cylinder/include)
include_directories(${RT024_MAP_LIB_ROOT}/include)
```

- [ ] **Step 4: 从 `virtual_camera_core` 源列表中移除 4v 和外部 map 依赖源码**

将 `foreach(source_file ...)` 调整为只保留 7v 相关源码，并加入新的 `src/remap_generator.cpp`：

```cmake
foreach(source_file
    src/json_utils.cpp
    src/pipeline_config.cpp
    src/calibration_loader.cpp
    src/undistort_processor.cpp
    src/virtual_camera_processor.cpp
    src/task_builder.cpp
    src/map_generator.cpp
    src/remap_generator.cpp
    src/json_writer.cpp
    src/verifier.cpp
)
```

删除：

```cmake
src/four_view_runner.cpp
third_party/gdc_add_cylinder/src/bin_file_io.cpp
third_party/gdc_add_cylinder/src/camera_maps_generator.cpp
third_party/gdc_add_cylinder/src/camera_params_loader.cpp
third_party/gdc_add_cylinder/src/config_loader.cpp
third_party/gdc_add_cylinder/src/cylinder_map.cpp
third_party/gdc_add_cylinder/src/data_types.cpp
third_party/gdc_add_cylinder/src/simple_json_parser.cpp
third_party/gdc_add_cylinder/src/weight_calculator.cpp
```

- [ ] **Step 5: 从链接库中移除外部 map 动态库**

保留：

```cmake
target_link_libraries(virtual_camera_core PUBLIC
    ${VENDORED_OPENCV_ROOT}/lib/libopencv_world.a
    ${VENDORED_OPENCV_ROOT}/lib/libzlib.a
    ${VENDORED_OPENCV_ROOT}/lib/libtegra_hal.a
    ${VENDORED_OPENCV_ROOT}/lib/liblibjasper.a
    ${VENDORED_OPENCV_ROOT}/lib/liblibjpeg-turbo.a
    ${VENDORED_OPENCV_ROOT}/lib/liblibpng.a
    ${VENDORED_OPENCV_ROOT}/lib/liblibtiff.a
    ${VENDORED_OPENCV_ROOT}/lib/liblibwebp.a
    pthread
    rt
    dl
    yaml-cpp
)
```

删除：

```cmake
${RT024_MAP_LIB_ROOT}/lib/libgen_vc_map_lib.so.1.0
${RT024_MAP_LIB_ROOT}/lib/libgdc_bin_generator.so
${RT024_MAP_LIB_ROOT}/lib/libgdc.so
```

- [ ] **Step 6: 先做一次配置生成，确认构建入口不再引用工作区外路径**

Run: `rm -rf build && cmake -S . -B build`

Expected:
- configure 成功
- `build/CMakeCache.txt` 中不再出现 `/workspace/icv_vc_bin_lib`
- `build/CMakeCache.txt` 中不再出现 `/workspace/gen_vc_bin_lib_test`

- [ ] **Step 7: 提交依赖迁移骨架**

```bash
git add third_party CMakeLists.txt scripts/build.sh
git commit -m "build: vendor 7v third-party dependencies"
```

### Task 2: 删除 4v 入口、文件和测试

**Files:**
- Delete: `src/four_view_runner.cpp`
- Delete: `include/virtual_camera/four_view_runner.h`
- Delete: `tests/test_four_view_config.cpp`
- Delete: `tests/test_four_view_camera_params.cpp`
- Delete: `configs/config_4v.yaml`
- Delete: `scripts/run_4v_verify.sh`
- Delete: `third_party/gdc_add_cylinder/`
- Modify: `CMakeLists.txt`
- Modify: `README.md`

- [ ] **Step 1: 删除 4v 代码和辅助资源**

要删除的路径：

```text
src/four_view_runner.cpp
include/virtual_camera/four_view_runner.h
tests/test_four_view_config.cpp
tests/test_four_view_camera_params.cpp
configs/config_4v.yaml
scripts/run_4v_verify.sh
third_party/gdc_add_cylinder
```

Expected: 这些文件从工作树中移除，且没有其他 7v 文件被误删。

- [ ] **Step 2: 清理 `CMakeLists.txt` 中的 4v test 目标**

删除这两段：

```cmake
if(EXISTS ${PROJECT_SOURCE_DIR}/tests/test_four_view_config.cpp)
    add_executable(test_four_view_config tests/test_four_view_config.cpp)
    target_link_libraries(test_four_view_config PRIVATE virtual_camera_core)
endif()

if(EXISTS ${PROJECT_SOURCE_DIR}/tests/test_four_view_camera_params.cpp)
    add_executable(test_four_view_camera_params tests/test_four_view_camera_params.cpp)
    target_link_libraries(test_four_view_camera_params PRIVATE virtual_camera_core)
endif()
```

- [ ] **Step 3: 搜索残留的 4v 入口引用**

Run: `grep -RIn "four_view\\|generate-4v\\|config_4v\\|run_4v" CMakeLists.txt src include tests configs scripts README.md`

Expected:
- 输出只允许出现在你计划更新的 `README.md`
- 不应再有可执行入口、测试目标或编译引用

- [ ] **Step 4: 更新 `README.md`，声明本分支只支持 7v/RT024**

替换或删除 4v 文案，写入以下说明：

```md
当前分支只保留 RT024 7v 数据处理流程：

- `process_undistort`
- `process_virtual_camera`

不再提供 4v 生成入口与相关脚本。
```

- [ ] **Step 5: 编译一次，确认删除 4v 后没有悬挂引用**

Run: `cmake --build build -j"$(nproc)"`

Expected:
- 编译能继续推进到 `undistort_processor.cpp` / `virtual_camera_processor.cpp`
- 如果此时因为 `<gen_vc_map.hpp>` 缺失而失败，视为符合预期
- 不应出现 `four_view_runner` 未定义或 `gdc_add_cylinder` 头文件找不到的错误

- [ ] **Step 6: 提交 4v 清理**

```bash
git add -A
git commit -m "refactor: remove 4v pipeline artifacts"
```

### Task 3: 新增仓内 remap 生成模块和单元测试

**Files:**
- Create: `include/virtual_camera/remap_generator.h`
- Create: `src/remap_generator.cpp`
- Create: `tests/test_remap_generator.cpp`
- Modify: `CMakeLists.txt`
- Test: `tests/test_remap_generator.cpp`

- [ ] **Step 1: 先写 `remap_generator` 头文件接口**

在 `include/virtual_camera/remap_generator.h` 中定义：

```cpp
#pragma once

#include "virtual_camera/pipeline_config.h"
#include "virtual_camera/types.h"

#include <opencv2/core.hpp>

#include <string>

namespace vc {

struct UndistortMaps {
  cv::Mat map_x;
  cv::Mat map_y;
};

struct VirtualCameraMaps {
  cv::Mat map_x;
  cv::Mat map_y;
  cv::Mat src_map_x;
  cv::Mat src_map_y;
};

UndistortMaps GenerateUndistortMaps(const CalibrationParam& calibration,
                                    const NewIntrinsicConfig& new_intrinsic,
                                    int distort_model);

VirtualCameraMaps GenerateVirtualCameraMaps(
    const CalibrationParam& calibration,
    const VirtualCameraTaskConfig& task,
    int distort_model);

void SaveFloatMapFile(const std::string& path, const cv::Mat& map);

}  // namespace vc
```

- [ ] **Step 2: 先写失败测试，固定输出尺寸和类型**

在 `tests/test_remap_generator.cpp` 中先写两个最小测试：

```cpp
void TestGenerateUndistortMapsReturnsFloatMaps() {
  vc::CalibrationParam calibration;
  calibration.intrinsic_matrix <<
      1000.0, 0.0, 960.0,
      0.0, 1000.0, 540.0,
      0.0, 0.0, 1.0;
  calibration.dist_data = {0.01, -0.02, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
  calibration.image_width = 1920;
  calibration.image_height = 1080;

  vc::NewIntrinsicConfig intrinsic;
  intrinsic.focal_u = 900.0;
  intrinsic.focal_v = 900.0;
  intrinsic.center_u = 960.0;
  intrinsic.center_v = 540.0;
  intrinsic.image_width = 1920;
  intrinsic.image_height = 1080;
  intrinsic.center = 1;

  const vc::UndistortMaps maps =
      vc::GenerateUndistortMaps(calibration, intrinsic, 0);

  Expect(maps.map_x.type() == CV_32FC1, "map_x should be CV_32FC1");
  Expect(maps.map_y.type() == CV_32FC1, "map_y should be CV_32FC1");
  Expect(maps.map_x.rows == 1080 && maps.map_x.cols == 1920,
         "map_x size should match output");
}
```

```cpp
void TestSaveFloatMapFileWritesExpectedBytes() {
  const std::filesystem::path root =
      std::filesystem::path("build/test_tmp/remap_generator");
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);

  cv::Mat map(2, 2, CV_32FC1);
  map.at<float>(0, 0) = 1.0f;
  map.at<float>(0, 1) = 2.0f;
  map.at<float>(1, 0) = 3.0f;
  map.at<float>(1, 1) = 4.0f;

  const std::filesystem::path output = root / "map.bin";
  vc::SaveFloatMapFile(output.string(), map);
  Expect(std::filesystem::file_size(output) == 4 * sizeof(float),
         "saved map should contain raw float payload");
}
```

- [ ] **Step 3: 把新测试目标接入 `CMakeLists.txt`**

添加：

```cmake
if(EXISTS ${PROJECT_SOURCE_DIR}/tests/test_remap_generator.cpp)
    add_executable(test_remap_generator tests/test_remap_generator.cpp)
    target_link_libraries(test_remap_generator PRIVATE virtual_camera_core)
endif()
```

- [ ] **Step 4: 跑新测试，确认当前因未实现而失败**

Run: `cmake --build build -j"$(nproc)" --target test_remap_generator`

Expected: FAIL，错误应为 `GenerateUndistortMaps` / `SaveFloatMapFile` 未定义或链接失败。

- [ ] **Step 5: 写最小实现，先覆盖去畸变 map 和文件落盘**

在 `src/remap_generator.cpp` 中先补最小实现：

```cpp
#include "virtual_camera/remap_generator.h"

#include "virtual_camera/map_generator.h"
#include "virtual_camera/json_utils.h"

#include <opencv2/calib3d.hpp>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace vc {

namespace {

cv::Mat BuildCameraMatrix(const Eigen::Matrix3d& intrinsic) {
  cv::Mat matrix(3, 3, CV_64FC1);
  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 3; ++col) {
      matrix.at<double>(row, col) = intrinsic(row, col);
    }
  }
  return matrix;
}

cv::Mat BuildDistortion(const std::vector<double>& distortion, int count) {
  std::vector<double> values(count, 0.0);
  for (int index = 0; index < count && index < static_cast<int>(distortion.size()); ++index) {
    values[index] = distortion[index];
  }
  return cv::Mat(values, true).reshape(0, count);
}

}  // namespace

UndistortMaps GenerateUndistortMaps(const CalibrationParam& calibration,
                                    const NewIntrinsicConfig& new_intrinsic,
                                    int distort_model) {
  cv::Mat camera_matrix = BuildCameraMatrix(calibration.intrinsic_matrix);
  cv::Mat new_camera = cv::Mat::eye(3, 3, CV_64FC1);
  new_camera.at<double>(0, 0) = new_intrinsic.focal_u;
  new_camera.at<double>(1, 1) = new_intrinsic.focal_v;
  new_camera.at<double>(0, 2) = new_intrinsic.center == 0
      ? calibration.intrinsic_matrix(0, 2)
      : new_intrinsic.center_u;
  new_camera.at<double>(1, 2) = new_intrinsic.center == 0
      ? calibration.intrinsic_matrix(1, 2)
      : new_intrinsic.center_v;

  UndistortMaps maps;
  if (distort_model == 1) {
    cv::fisheye::initUndistortRectifyMap(
        camera_matrix, BuildDistortion(calibration.dist_data, 4), cv::Mat::eye(3, 3, CV_64FC1),
        new_camera, cv::Size(new_intrinsic.image_width, new_intrinsic.image_height),
        CV_32FC1, maps.map_x, maps.map_y);
  } else {
    cv::initUndistortRectifyMap(
        camera_matrix, BuildDistortion(calibration.dist_data, 8), cv::Mat::eye(3, 3, CV_64FC1),
        new_camera, cv::Size(new_intrinsic.image_width, new_intrinsic.image_height),
        CV_32FC1, maps.map_x, maps.map_y);
  }
  return maps;
}

void SaveFloatMapFile(const std::string& path, const cv::Mat& map) {
  EnsureDirectory(std::filesystem::path(path).parent_path().string());
  cv::Mat contiguous = map.isContinuous() ? map : map.clone();
  std::ofstream output(path, std::ios::binary);
  if (!output.is_open()) {
    throw std::runtime_error("failed to write map: " + path);
  }
  output.write(reinterpret_cast<const char*>(contiguous.ptr<float>(0)),
               static_cast<std::streamsize>(contiguous.total() * sizeof(float)));
}
```

- [ ] **Step 6: 补齐虚拟相机 map 实现**

继续在 `src/remap_generator.cpp` 中添加：

```cpp
VirtualCameraMaps GenerateVirtualCameraMaps(
    const CalibrationParam& calibration,
    const VirtualCameraTaskConfig& task,
    int distort_model) {
  VirtualParam virtual_param;
  virtual_param.virtual_width = task.new_intrinsic.image_width;
  virtual_param.virtual_height = task.new_intrinsic.image_height;
  virtual_param.virtual_fov = task.new_intrinsic.fov;
  virtual_param.virtual_yaw = task.new_extrinsics.yaw;
  virtual_param.virtual_pitch = task.new_extrinsics.pitch;
  virtual_param.virtual_roll = task.new_extrinsics.roll;
  virtual_param.tx = calibration.extrinsic_matrix(0, 3);
  virtual_param.ty = calibration.extrinsic_matrix(1, 3);
  virtual_param.tz = calibration.extrinsic_matrix(2, 3);
  virtual_param.image_width = task.image_width;
  virtual_param.image_height = task.image_height;
  virtual_param.fov = task.fov;
  virtual_param.camera_id = task.camera_id;

  MapGenerator generator(calibration, virtual_param);
  const bool fisheye_model = distort_model == 1;
  const std::vector<cv::Mat> virtual_maps =
      generator.CreateVirtualMap(fisheye_model);

  VirtualCameraMaps maps;
  maps.map_x = virtual_maps.at(0);
  maps.map_y = virtual_maps.at(1);

  Eigen::Matrix3d resized_intrinsic = Eigen::Matrix3d::Identity();
  const std::vector<cv::Mat> src_maps =
      generator.CreateResizeMap(&resized_intrinsic);
  maps.src_map_x = src_maps.at(0);
  maps.src_map_y = src_maps.at(1);
  return maps;
}
```

- [ ] **Step 7: 运行新测试，确认通过**

Run:
- `cmake --build build -j"$(nproc)" --target test_remap_generator`
- `./build/test_remap_generator`

Expected:
- build 成功
- `test_remap_generator` 退出码为 0

- [ ] **Step 8: 提交 remap 模块**

```bash
git add include/virtual_camera/remap_generator.h src/remap_generator.cpp tests/test_remap_generator.cpp CMakeLists.txt
git commit -m "feat: add internal 7v remap generator"
```

### Task 4: 切换去畸变流程到仓内 remap 实现

**Files:**
- Modify: `src/undistort_processor.cpp`
- Modify: `tests/test_undistort_processor.cpp`
- Test: `tests/test_undistort_processor.cpp`

- [ ] **Step 1: 先改头文件引用，移除外部 `<gen_vc_map.hpp>`**

将：

```cpp
#include <gen_vc_map.hpp>
```

替换为：

```cpp
#include "virtual_camera/remap_generator.h"
```

- [ ] **Step 2: 用 `GenerateUndistortMaps` 替换外部 map 生成**

将当前：

```cpp
  double k_array[3][3];
  double d_array[8];
  double new_k_array[4];
  FillCameraMatrix(calibration.intrinsic_matrix, k_array);
  FillDistortion(calibration.dist_data, d_array);
  new_k_array[0] = task.new_intrinsic.focal_u;
  new_k_array[1] = task.new_intrinsic.center == 0 ? calibration.intrinsic_matrix(0, 2)
                                                   : task.new_intrinsic.center_u;
  new_k_array[2] = task.new_intrinsic.focal_v;
  new_k_array[3] = task.new_intrinsic.center == 0 ? calibration.intrinsic_matrix(1, 2)
                                                   : task.new_intrinsic.center_v;

  cv::Mat map_x;
  cv::Mat map_y;
  if (config.distort_model == 1) {
    gen_undis_map_kb(...);
  } else {
    gen_undis_map(...);
  }
```

替换为：

```cpp
  const UndistortMaps maps =
      GenerateUndistortMaps(calibration, task.new_intrinsic, config.distort_model);
  const cv::Mat& map_x = maps.map_x;
  const cv::Mat& map_y = maps.map_y;
```

并删除 `FillCameraMatrix`、`FillDistortion` 这类不再使用的本地辅助函数。

- [ ] **Step 3: 为真实数据回归补一条最小断言**

在 `tests/test_undistort_processor.cpp` 增加一个成功路径测试，至少验证 JSON 和图片目录会创建：

```cpp
void TestRunUndistortPipelineWritesOutputsForRealDataset() {
  const std::filesystem::path root = MakeTestRoot("real_dataset");
  vc::PipelineConfig config = MakeBaseConfig(root);
  config.undistort_tasks.push_back(
      MakeTask("calib_camera_front_wide_to_car.json", "front_wide/"));

  vc::RunUndistortPipeline(config);

  Expect(std::filesystem::exists(root / "calib_undistortion" /
                                 "calib_camera_front_wide_to_car.json"),
         "undistort json should exist");
  Expect(std::filesystem::exists(root / "image_undistortion" / "front_wide"),
         "undistort image dir should exist");
}
```

- [ ] **Step 4: 先跑去畸变测试**

Run:
- `cmake --build build -j"$(nproc)" --target test_undistort_processor`
- `./build/test_undistort_processor`

Expected:
- 所有现有碰撞测试继续通过
- 新增成功路径测试通过

- [ ] **Step 5: 提交去畸变切换**

```bash
git add src/undistort_processor.cpp tests/test_undistort_processor.cpp
git commit -m "refactor: internalize undistort remap generation"
```

### Task 5: 切换虚拟相机流程到仓内 remap 实现

**Files:**
- Modify: `src/virtual_camera_processor.cpp`
- Modify: `tests/test_virtual_camera_processor.cpp`
- Test: `tests/test_virtual_camera_processor.cpp`

- [ ] **Step 1: 移除 `<gen_vc_map.hpp>` 并接入新头文件**

将：

```cpp
#include <gen_vc_map.hpp>
```

替换为：

```cpp
#include "virtual_camera/remap_generator.h"
```

- [ ] **Step 2: 用 `GenerateVirtualCameraMaps` 和 `SaveFloatMapFile` 替换旧实现**

将：

```cpp
  double k_array[3][3];
  double d_array[8];
  double rt_array[4][4];
  FillCameraMatrix(...);
  FillDistortion(...);
  FillExtrinsic(...);

  cv::Mat map_x;
  cv::Mat map_y;
  cv::Mat src_map_x;
  cv::Mat src_map_y;
  if (config.distort_model == 1) {
    gen_vc_map_kb(...);
  } else {
    gen_vc_map(...);
  }

  SaveFloatMap(...);
```

替换为：

```cpp
  const VirtualCameraMaps maps =
      GenerateVirtualCameraMaps(calibration, task, config.distort_model);

  const std::filesystem::path map_root =
      std::filesystem::path(config.output_root) / config.paths.vc_gdcbin_dir_path;
  SaveFloatMapFile((map_root / task.vc_mapx_name).string(), maps.map_x);
  SaveFloatMapFile((map_root / task.vc_mapy_name).string(), maps.map_y);
  SaveFloatMapFile((map_root / task.src2vc_mapx_name).string(), maps.src_map_x);
  SaveFloatMapFile((map_root / task.src2vc_mapy_name).string(), maps.src_map_y);
```

如果 `BuildVirtualParam()` 只剩一处使用，可以内联保留；若仍有价值则保留现有 helper。

- [ ] **Step 3: 删除不再需要的本地辅助函数**

删除：

```cpp
void FillCameraMatrix(...)
void FillExtrinsic(...)
void FillDistortion(...)
void SaveFloatMap(...)
```

保留：

```cpp
VirtualParam BuildVirtualParam(...)
```

仅当它仍用于 `WriteRt024VirtualJson(...)` 所需计算。

- [ ] **Step 4: 为真实数据增加一条成功路径测试**

在 `tests/test_virtual_camera_processor.cpp` 增加：

```cpp
void TestRunVirtualCameraPipelineWritesOutputsForRealDataset() {
  const std::filesystem::path root = MakeTestRoot("real_dataset");
  vc::PipelineConfig config = MakeBaseConfig(root);
  config.virtual_camera_parallelism = 1;
  config.virtual_tasks.push_back(MakeValidTask());

  vc::RunVirtualCameraPipeline(config);

  Expect(std::filesystem::exists(root / "calib_virtual_camera" /
                                 "calib_cam_front_wide_fov110.json"),
         "virtual camera json should exist");
  Expect(std::filesystem::exists(root / "vc_gdcbin_dir_path" /
                                 "fw110_vc_mapX.bin"),
         "virtual camera map should exist");
  Expect(std::filesystem::exists(root / "image_virtual_camera" /
                                 "front_wide_110"),
         "virtual camera image dir should exist");
}
```

- [ ] **Step 5: 运行虚拟相机测试**

Run:
- `cmake --build build -j"$(nproc)" --target test_virtual_camera_processor`
- `./build/test_virtual_camera_processor`

Expected:
- 现有冲突检测测试通过
- 新增成功路径测试通过

- [ ] **Step 6: 提交虚拟相机切换**

```bash
git add src/virtual_camera_processor.cpp tests/test_virtual_camera_processor.cpp
git commit -m "refactor: internalize virtual camera remap generation"
```

### Task 6: 完成文档、回归测试和结果核对

**Files:**
- Modify: `README.md`
- Modify: `configs/config_rt024.json`
- Modify: `configs/config_rt024_parallel_run.json`
- Test: `build/virtual_camera_tool`

- [ ] **Step 1: 更新 README 的依赖和运行说明**

将旧的外部 `LD_LIBRARY_PATH` 说明删除，替换为：

```md
当前项目直接使用仓库内 `third_party` 下的 `opencv`、`eigen`、`nlohmann_json` 构建。

不再需要：

```bash
export LD_LIBRARY_PATH=/workspace/gen_vc_bin_lib_test/deps/x86/gen_vc_map_lib/lib:${LD_LIBRARY_PATH:-}
```
```

并补充：

```md
当前分支只对以下内容做真值比较：

- `calib_undistortion`
- `calib_virtual_camera`
- `vc_gdcbin_dir_path`

图片只要求成功生成。
```

- [ ] **Step 2: 检查 RT024 配置文件不再暗示 4v 或外部 map 动态库**

Run:
- `grep -RIn "4v\\|LD_LIBRARY_PATH\\|gen_vc_map_lib\\|gdc_bin_generator" configs README.md scripts`

Expected: 不再出现运行时依赖外部 map 库的说明。

- [ ] **Step 3: 完整构建所有当前保留目标**

Run: `bash scripts/build.sh`

Expected:
- `build/virtual_camera_tool`
- `build/test_calibration_loader`
- `build/test_jobs`
- `build/test_parallel_executor`
- `build/test_pipeline_config`
- `build/test_pipeline_runner`
- `build/test_remap_generator`
- `build/test_rt024_json_writer`
- `build/test_rt024_verifier`
- `build/test_task_builder`
- `build/test_undistort_processor`
- `build/test_verifier`
- `build/test_virtual_camera_processor`

全部生成成功。

- [ ] **Step 4: 运行单元测试**

Run:
- `./build/test_calibration_loader`
- `./build/test_jobs`
- `./build/test_parallel_executor`
- `./build/test_pipeline_config`
- `./build/test_pipeline_runner`
- `./build/test_remap_generator`
- `./build/test_rt024_json_writer`
- `./build/test_rt024_verifier`
- `./build/test_task_builder`
- `./build/test_undistort_processor`
- `./build/test_verifier`
- `./build/test_virtual_camera_processor`

Expected: 全部退出码为 0。

- [ ] **Step 5: 运行 RT024 真值回归**

Run:

```bash
./build/virtual_camera_tool configs/config_rt024.json
```

Expected:
- 程序退出码为 0
- stdout 包含 `verification passed`

- [ ] **Step 6: 如果串行回归通过，再跑并行配置**

Run:

```bash
./build/virtual_camera_tool configs/config_rt024_parallel_run.json
```

Expected:
- 程序退出码为 0
- stdout 包含 `verification passed`

- [ ] **Step 7: 记录工作树结果并提交**

```bash
git status --short
git add README.md configs/config_rt024.json configs/config_rt024_parallel_run.json
git commit -m "docs: update 7v runtime and verification guidance"
```

- [ ] **Step 8: 最终检查工作树**

Run:
- `git status --short`
- `grep -RIn "/workspace/icv_vc_bin_lib\\|/workspace/gen_vc_bin_lib_test\\|gen_vc_map.hpp\\|gdc_add_cylinder\\|generate-4v" . --exclude-dir=.git --exclude-dir=build`

Expected:
- 除用户自己的未跟踪目录外，工作树干净或只剩已知非本任务文件
- 搜索结果中不再出现外部依赖和 4v 入口残留
