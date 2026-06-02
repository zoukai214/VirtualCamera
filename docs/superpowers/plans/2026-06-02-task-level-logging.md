# RT024 Task-Level Logging Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 RT024 主流程增加由 `showinfo` 控制的任务级运行日志，并保持脚本入口、产物输出和最终校验行为不变。

**Architecture:** 先增加一个小型日志辅助模块，统一处理 `[INFO]` 输出、消息格式和并发写 `stdout` 的互斥。然后在 `main`、`undistort` 流水线和 `virtual_camera` 流水线边界接入日志，最后用现有真实数据测试和脚本回归验证端到端输出。

**Tech Stack:** C++17、标准库 `chrono`/`mutex`/`sstream`、现有自定义测试可执行程序、CMake

---

## File Structure

- `include/virtual_camera/logging.h`
  - 新增轻量日志接口声明。
  - 提供消息构造函数、`LogInfo()` 和耗时辅助函数。

- `src/logging.cpp`
  - 新增日志实现。
  - 统一输出 `[INFO]` 前缀，并使用 `std::mutex` 保证并发场景下一次只写一整行。

- `src/main.cpp`
  - 在进入顶层流水线前输出整体入口日志。

- `src/undistort_processor.cpp`
  - 增加 `undistort` 流水线和单任务开始/结束日志。

- `src/virtual_camera_processor.cpp`
  - 增加 `virtual_camera` 流水线和单任务开始/结束日志。

- `tests/test_logging.cpp`
  - 新增纯日志单元测试。
  - 验证消息格式、开关行为和 `stdout` 输出格式。

- `tests/test_undistort_processor.cpp`
  - 扩展真实数据测试，验证 `showinfo=1/0` 时的 `undistort` 日志行为。

- `tests/test_virtual_camera_processor.cpp`
  - 扩展真实数据测试，验证 `showinfo=1/0` 时的 `virtual_camera` 日志行为。

- `CMakeLists.txt`
  - 把 `src/logging.cpp` 编进 `virtual_camera_core`。
  - 注册新的 `test_logging` 目标。

- `README.md`
  - 说明 `showinfo` 现在控制任务级运行日志。

### Task 1: Add Logging Helper And Unit Tests

**Files:**
- Create: `include/virtual_camera/logging.h`
- Create: `src/logging.cpp`
- Create: `tests/test_logging.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing logging helper test**

在 `tests/test_logging.cpp` 写完整测试文件，先定义日志辅助 API 的预期行为：

```cpp
#include "virtual_camera/logging.h"
#include "virtual_camera/pipeline_config.h"
#include "virtual_camera/runtime_args.h"

#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

void Expect(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

std::string CaptureStdout(const std::function<void()>& fn) {
  std::ostringstream buffer;
  std::streambuf* previous = std::cout.rdbuf(buffer.rdbuf());
  try {
    fn();
  } catch (...) {
    std::cout.rdbuf(previous);
    throw;
  }
  std::cout.rdbuf(previous);
  return buffer.str();
}

void TestLogInfoPrintsSinglePrefixedLine() {
  const std::string output = CaptureStdout([]() {
    vc::LogInfo(true, "undistort start: tasks=7 parallelism=7");
  });
  Expect(output == "[INFO] undistort start: tasks=7 parallelism=7\n",
         "LogInfo should print one prefixed line");
}

void TestLogInfoSkipsDisabledMessages() {
  const std::string output =
      CaptureStdout([]() { vc::LogInfo(false, "should not print"); });
  Expect(output.empty(), "LogInfo should stay silent when disabled");
}

void TestBuildRt024PipelineStartMessageIncludesRuntimeRoots() {
  vc::Rt024RuntimeArgs args;
  args.dataset_root = "/tmp/dataset";
  args.config_path = "configs/config_rt024_parallel_run.json";
  args.debug = true;

  vc::PipelineConfig config;
  config.output_root = "/tmp/output";

  const std::string message = vc::BuildRt024PipelineStartMessage(args, config);
  Expect(message.find("pipeline start:") != std::string::npos,
         "start message should include label");
  Expect(message.find("dataset=/tmp/dataset") != std::string::npos,
         "start message should include dataset root");
  Expect(message.find("config=configs/config_rt024_parallel_run.json") !=
             std::string::npos,
         "start message should include config path");
  Expect(message.find("output=/tmp/output") != std::string::npos,
         "start message should include output root");
  Expect(message.find("debug=true") != std::string::npos,
         "start message should include debug flag");
}

void TestBuildVirtualTaskStartMessageIncludesTaskIdentity() {
  vc::VirtualCameraTaskConfig task;
  task.file_prefix = "fw110";
  task.save_dir = "front_wide_110/";
  task.calib_json = "calib_cam_front_wide_fov110.json";

  const std::string message = vc::BuildVirtualTaskStartMessage(task);
  Expect(message.find("virtual task start:") != std::string::npos,
         "virtual task log should include label");
  Expect(message.find("prefix=fw110") != std::string::npos,
         "virtual task log should include prefix");
  Expect(message.find("save_dir=front_wide_110/") != std::string::npos,
         "virtual task log should include save_dir");
  Expect(message.find("calib=calib_cam_front_wide_fov110.json") !=
             std::string::npos,
         "virtual task log should include calib json");
}

}  // namespace

int main() {
  TestLogInfoPrintsSinglePrefixedLine();
  TestLogInfoSkipsDisabledMessages();
  TestBuildRt024PipelineStartMessageIncludesRuntimeRoots();
  TestBuildVirtualTaskStartMessageIncludesTaskIdentity();
  return 0;
}
```

- [ ] **Step 2: Run the new test target and verify it fails**

Run:

```bash
cmake -S . -B build
cmake --build build --target test_logging -j2
```

Expected:

- 第一次构建失败。
- 典型失败形态之一：
  - `No rule to make target 'test_logging'`
  - `fatal error: virtual_camera/logging.h: No such file or directory`

- [ ] **Step 3: Implement the minimal logging helper and wire it into CMake**

先在 `include/virtual_camera/logging.h` 写声明：

```cpp
#pragma once

#include "virtual_camera/pipeline_config.h"
#include "virtual_camera/runtime_args.h"

#include <chrono>
#include <string>

namespace vc {

void LogInfo(bool enabled, const std::string& message);

std::string BuildRt024PipelineStartMessage(const Rt024RuntimeArgs& args,
                                           const PipelineConfig& config);
std::string BuildUndistortPipelineStartMessage(const PipelineConfig& config);
std::string BuildUndistortPipelineDoneMessage(long long elapsed_ms);
std::string BuildUndistortTaskStartMessage(const UndistortTaskConfig& task);
std::string BuildUndistortTaskDoneMessage(const UndistortTaskConfig& task,
                                          long long elapsed_ms);
std::string BuildVirtualCameraPipelineStartMessage(const PipelineConfig& config);
std::string BuildVirtualCameraPipelineDoneMessage(long long elapsed_ms);
std::string BuildVirtualTaskStartMessage(const VirtualCameraTaskConfig& task);
std::string BuildVirtualTaskDoneMessage(const VirtualCameraTaskConfig& task,
                                        long long elapsed_ms);

inline long long ElapsedMilliseconds(
    const std::chrono::steady_clock::time_point& start_time) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now() - start_time)
      .count();
}

}  // namespace vc
```

再在 `src/logging.cpp` 写实现：

```cpp
#include "virtual_camera/logging.h"

#include <iostream>
#include <mutex>
#include <sstream>

namespace vc {
namespace {

std::mutex& LogMutex() {
  static std::mutex mutex;
  return mutex;
}

std::string BoolText(bool value) { return value ? "true" : "false"; }

}  // namespace

void LogInfo(bool enabled, const std::string& message) {
  if (!enabled) {
    return;
  }
  std::lock_guard<std::mutex> lock(LogMutex());
  std::cout << "[INFO] " << message << "\n";
}

std::string BuildRt024PipelineStartMessage(const Rt024RuntimeArgs& args,
                                           const PipelineConfig& config) {
  std::ostringstream stream;
  stream << "pipeline start: dataset=" << args.dataset_root
         << " config=" << args.config_path
         << " output=" << config.output_root
         << " debug=" << BoolText(args.debug);
  return stream.str();
}

std::string BuildUndistortPipelineStartMessage(const PipelineConfig& config) {
  std::ostringstream stream;
  stream << "undistort start: tasks=" << config.undistort_tasks.size()
         << " parallelism=" << config.undistort_parallelism;
  return stream.str();
}

std::string BuildUndistortPipelineDoneMessage(long long elapsed_ms) {
  std::ostringstream stream;
  stream << "undistort done: elapsed_ms=" << elapsed_ms;
  return stream.str();
}

std::string BuildUndistortTaskStartMessage(const UndistortTaskConfig& task) {
  std::ostringstream stream;
  stream << "undistort task start: image_dir=" << task.image_dir
         << " calib=" << task.conf_json;
  return stream.str();
}

std::string BuildUndistortTaskDoneMessage(const UndistortTaskConfig& task,
                                          long long elapsed_ms) {
  std::ostringstream stream;
  stream << "undistort task done: image_dir=" << task.image_dir
         << " elapsed_ms=" << elapsed_ms;
  return stream.str();
}

std::string BuildVirtualCameraPipelineStartMessage(const PipelineConfig& config) {
  std::ostringstream stream;
  stream << "virtual_camera start: tasks=" << config.virtual_tasks.size()
         << " parallelism=" << config.virtual_camera_parallelism;
  return stream.str();
}

std::string BuildVirtualCameraPipelineDoneMessage(long long elapsed_ms) {
  std::ostringstream stream;
  stream << "virtual_camera done: elapsed_ms=" << elapsed_ms;
  return stream.str();
}

std::string BuildVirtualTaskStartMessage(const VirtualCameraTaskConfig& task) {
  std::ostringstream stream;
  stream << "virtual task start: prefix=" << task.file_prefix
         << " save_dir=" << task.save_dir
         << " calib=" << task.calib_json;
  return stream.str();
}

std::string BuildVirtualTaskDoneMessage(const VirtualCameraTaskConfig& task,
                                        long long elapsed_ms) {
  std::ostringstream stream;
  stream << "virtual task done: prefix=" << task.file_prefix
         << " save_dir=" << task.save_dir
         << " elapsed_ms=" << elapsed_ms;
  return stream.str();
}

}  // namespace vc
```

最后更新 `CMakeLists.txt`：

```cmake
foreach(source_file
    src/json_utils.cpp
    src/pipeline_config.cpp
    src/calibration_loader.cpp
    src/undistort_processor.cpp
    src/virtual_camera_processor.cpp
    src/task_builder.cpp
    src/map_generator.cpp
    src/json_writer.cpp
    src/verifier.cpp
    src/runtime_args.cpp
    src/logging.cpp
)
```

```cmake
if(EXISTS ${PROJECT_SOURCE_DIR}/tests/test_logging.cpp)
    add_executable(test_logging tests/test_logging.cpp)
    target_link_libraries(test_logging PRIVATE virtual_camera_core)
endif()
```

- [ ] **Step 4: Run the logging helper test and verify it passes**

Run:

```bash
cmake -S . -B build
cmake --build build --target test_logging -j2
./build/test_logging
```

Expected:

- `test_logging` 编译成功
- `./build/test_logging` 退出码为 `0`

- [ ] **Step 5: Commit the helper layer**

```bash
git add CMakeLists.txt include/virtual_camera/logging.h src/logging.cpp tests/test_logging.cpp
git commit -m "feat: add rt024 logging helpers"
```

### Task 2: Add Undistort Pipeline And Task Logs

**Files:**
- Modify: `src/undistort_processor.cpp`
- Modify: `tests/test_undistort_processor.cpp`

- [ ] **Step 1: Write the failing undistort logging tests**

在 `tests/test_undistort_processor.cpp` 增加 `CaptureStdout()` 和两条日志测试：

```cpp
#include <functional>
#include <iostream>
#include <sstream>
```

```cpp
std::string CaptureStdout(const std::function<void()>& fn) {
  std::ostringstream buffer;
  std::streambuf* previous = std::cout.rdbuf(buffer.rdbuf());
  try {
    fn();
  } catch (...) {
    std::cout.rdbuf(previous);
    throw;
  }
  std::cout.rdbuf(previous);
  return buffer.str();
}
```

```cpp
void TestRunUndistortPipelinePrintsTaskLogsWhenShowinfoEnabled() {
  const std::filesystem::path root = MakeTestRoot("logging_enabled");
  vc::PipelineConfig config = MakeBaseConfig(root);
  config.showinfo = 1;
  config.undistort_parallelism = 2;
  config.undistort_tasks.push_back(
      MakeTask("calib_camera_front_wide_to_car.json", "front_wide/"));

  const std::string output =
      CaptureStdout([&config]() { vc::RunUndistortPipeline(config); });

  Expect(output.find("[INFO] undistort start: tasks=1 parallelism=2\n") !=
             std::string::npos,
         "showinfo=1 should print undistort pipeline start");
  Expect(output.find("[INFO] undistort task start: image_dir=front_wide/ "
                     "calib=calib_camera_front_wide_to_car.json\n") !=
             std::string::npos,
         "showinfo=1 should print undistort task start");
  Expect(output.find("[INFO] undistort task done: image_dir=front_wide/ "
                     "elapsed_ms=") != std::string::npos,
         "showinfo=1 should print undistort task done");
  Expect(output.find("[INFO] undistort done: elapsed_ms=") !=
             std::string::npos,
         "showinfo=1 should print undistort pipeline done");
}

void TestRunUndistortPipelineSkipsTaskLogsWhenShowinfoDisabled() {
  const std::filesystem::path root = MakeTestRoot("logging_disabled");
  vc::PipelineConfig config = MakeBaseConfig(root);
  config.showinfo = 0;
  config.undistort_parallelism = 2;
  config.undistort_tasks.push_back(
      MakeTask("calib_camera_front_wide_to_car.json", "front_wide/"));

  const std::string output =
      CaptureStdout([&config]() { vc::RunUndistortPipeline(config); });
  Expect(output.empty(), "showinfo=0 should suppress undistort logs");
}
```

并在 `main()` 里补充调用：

```cpp
  TestRunUndistortPipelinePrintsTaskLogsWhenShowinfoEnabled();
  TestRunUndistortPipelineSkipsTaskLogsWhenShowinfoDisabled();
```

- [ ] **Step 2: Run the undistort processor test and verify it fails**

Run:

```bash
cmake --build build --target test_undistort_processor -j2
./build/test_undistort_processor
```

Expected:

- 编译成功
- 运行失败，失败信息指向新增断言
- 典型失败文案：`showinfo=1 should print undistort pipeline start`

- [ ] **Step 3: Add undistort pipeline and task logs**

在 `src/undistort_processor.cpp` 加入日志头文件和计时：

```cpp
#include "virtual_camera/logging.h"

#include <chrono>
```

在 `RunUndistortTask()` 开头和结尾加入：

```cpp
void RunUndistortTask(const PipelineConfig& config, const UndistortTaskConfig& task) {
  const auto start_time = std::chrono::steady_clock::now();
  LogInfo(config.showinfo != 0, BuildUndistortTaskStartMessage(task));

  CalibrationParam calibration = LoadUndistortSourceCalibration(config, task);
  const UndistortMaps maps =
      GenerateUndistortMaps(calibration, task.new_intrinsic, config.distort_model);

  const std::filesystem::path json_output =
      std::filesystem::path(config.output_root) / config.paths.undistort_conf_dir_path /
      task.conf_json;
  WriteRt024UndistortJson(json_output.string(), calibration, task.new_intrinsic);

  const std::filesystem::path input_dir =
      std::filesystem::path(config.dataset_root) / config.paths.image_dir_path / task.image_dir;
  const std::filesystem::path output_dir =
      std::filesystem::path(config.output_root) / config.paths.undistort_image_dir_path /
      task.image_dir;
  EnsureDirectory(output_dir.string());
  for (const auto& path : ListFiles(input_dir)) {
    const cv::Mat image = cv::imread(path.string(), cv::IMREAD_COLOR);
    if (image.empty()) {
      throw std::runtime_error("failed to read image: " + path.string());
    }
    cv::Mat remapped;
    cv::remap(image, remapped, maps.map_x, maps.map_y, cv::INTER_LINEAR);
    if (!cv::imwrite((output_dir / path.filename()).string(), remapped)) {
      throw std::runtime_error("failed to write image: " +
                               (output_dir / path.filename()).string());
    }
  }

  LogInfo(config.showinfo != 0,
          BuildUndistortTaskDoneMessage(task, ElapsedMilliseconds(start_time)));
}
```

在 `RunUndistortPipeline()` 加入：

```cpp
void RunUndistortPipeline(const PipelineConfig& config) {
  const auto start_time = std::chrono::steady_clock::now();
  LogInfo(config.showinfo != 0, BuildUndistortPipelineStartMessage(config));

  if (config.undistort_parallelism <= 1) {
    for (const auto& task : config.undistort_tasks) {
      RunUndistortTask(config, task);
    }
    LogInfo(config.showinfo != 0,
            BuildUndistortPipelineDoneMessage(ElapsedMilliseconds(start_time)));
    return;
  }

  ValidateUndistortOutputs(config);

  std::vector<std::function<void()>> jobs;
  jobs.reserve(config.undistort_tasks.size());
  for (const auto& task : config.undistort_tasks) {
    jobs.push_back([&config, task]() { RunUndistortTask(config, task); });
  }
  RunJobs(jobs, config.undistort_parallelism);

  LogInfo(config.showinfo != 0,
          BuildUndistortPipelineDoneMessage(ElapsedMilliseconds(start_time)));
}
```

- [ ] **Step 4: Run the undistort logging tests and verify they pass**

Run:

```bash
cmake --build build --target test_undistort_processor -j2
./build/test_undistort_processor
```

Expected:

- `test_undistort_processor` 编译成功
- `./build/test_undistort_processor` 退出码为 `0`

- [ ] **Step 5: Commit the undistort logging change**

```bash
git add src/undistort_processor.cpp tests/test_undistort_processor.cpp
git commit -m "feat: add undistort progress logging"
```

### Task 3: Add Virtual Pipeline Logs, Main Start Log, And End-To-End Verification

**Files:**
- Modify: `src/main.cpp`
- Modify: `src/virtual_camera_processor.cpp`
- Modify: `tests/test_virtual_camera_processor.cpp`
- Modify: `README.md`

- [ ] **Step 1: Write the failing virtual logging tests**

在 `tests/test_virtual_camera_processor.cpp` 增加 `CaptureStdout()` 和两条日志测试：

```cpp
#include <functional>
#include <iostream>
#include <sstream>
```

```cpp
std::string CaptureStdout(const std::function<void()>& fn) {
  std::ostringstream buffer;
  std::streambuf* previous = std::cout.rdbuf(buffer.rdbuf());
  try {
    fn();
  } catch (...) {
    std::cout.rdbuf(previous);
    throw;
  }
  std::cout.rdbuf(previous);
  return buffer.str();
}
```

```cpp
void TestRunVirtualCameraPipelinePrintsTaskLogsWhenShowinfoEnabled() {
  const std::filesystem::path root = MakeTestRoot("logging_enabled");
  vc::PipelineConfig config = MakeBaseConfig(root);
  config.showinfo = 1;
  config.virtual_camera_parallelism = 2;
  config.virtual_tasks.push_back(MakeValidTask());

  const std::string output =
      CaptureStdout([&config]() { vc::RunVirtualCameraPipeline(config); });

  Expect(output.find("[INFO] virtual_camera start: tasks=1 parallelism=2\n") !=
             std::string::npos,
         "showinfo=1 should print virtual pipeline start");
  Expect(output.find("[INFO] virtual task start: prefix=fw110 "
                     "save_dir=front_wide_110/ "
                     "calib=calib_cam_front_wide_fov110.json\n") !=
             std::string::npos,
         "showinfo=1 should print virtual task start");
  Expect(output.find("[INFO] virtual task done: prefix=fw110 "
                     "save_dir=front_wide_110/ elapsed_ms=") !=
             std::string::npos,
         "showinfo=1 should print virtual task done");
  Expect(output.find("[INFO] virtual_camera done: elapsed_ms=") !=
             std::string::npos,
         "showinfo=1 should print virtual pipeline done");
}

void TestRunVirtualCameraPipelineSkipsTaskLogsWhenShowinfoDisabled() {
  const std::filesystem::path root = MakeTestRoot("logging_disabled");
  vc::PipelineConfig config = MakeBaseConfig(root);
  config.showinfo = 0;
  config.virtual_camera_parallelism = 2;
  config.virtual_tasks.push_back(MakeValidTask());

  const std::string output =
      CaptureStdout([&config]() { vc::RunVirtualCameraPipeline(config); });
  Expect(output.empty(), "showinfo=0 should suppress virtual logs");
}
```

并在 `main()` 里补充调用：

```cpp
  TestRunVirtualCameraPipelinePrintsTaskLogsWhenShowinfoEnabled();
  TestRunVirtualCameraPipelineSkipsTaskLogsWhenShowinfoDisabled();
```

- [ ] **Step 2: Run the virtual processor test and verify it fails**

Run:

```bash
cmake --build build --target test_virtual_camera_processor -j2
./build/test_virtual_camera_processor
```

Expected:

- 编译成功
- 运行失败，失败信息指向新增断言
- 典型失败文案：`showinfo=1 should print virtual pipeline start`

- [ ] **Step 3: Add virtual pipeline logs, main start log, and README note**

在 `src/virtual_camera_processor.cpp` 加入：

```cpp
#include "virtual_camera/logging.h"

#include <chrono>
```

```cpp
void RunVirtualCameraTask(const PipelineConfig& config,
                          const VirtualCameraTaskConfig& task) {
  const auto start_time = std::chrono::steady_clock::now();
  LogInfo(config.showinfo != 0, BuildVirtualTaskStartMessage(task));

  CalibrationParam calibration = LoadVirtualSourceCalibration(config, task);
  const VirtualCameraMaps maps =
      GenerateVirtualCameraMaps(calibration, task, config.distort_model);

  const std::filesystem::path map_root =
      std::filesystem::path(config.output_root) / config.paths.vc_gdcbin_dir_path;
  SaveFloatMapFile((map_root / task.vc_mapx_name).string(), maps.map_x);
  SaveFloatMapFile((map_root / task.vc_mapy_name).string(), maps.map_y);
  SaveFloatMapFile((map_root / task.src2vc_mapx_name).string(), maps.src_map_x);
  SaveFloatMapFile((map_root / task.src2vc_mapy_name).string(), maps.src_map_y);

  VirtualParam virtual_param = BuildVirtualParam(task, calibration);
  MapGenerator generator(calibration, virtual_param);
  const std::filesystem::path json_output =
      std::filesystem::path(config.output_root) / config.paths.vc_conf_dir_path /
      task.calib_json;
  WriteRt024VirtualJson(json_output.string(), generator.virtual_intrinsic(),
                        generator.virtual_extrinsic(), std::vector<double>(8, 0.0));

  const std::filesystem::path input_dir =
      std::filesystem::path(config.dataset_root) / config.paths.image_dir_path / task.image_dir;
  const std::filesystem::path output_dir =
      std::filesystem::path(config.output_root) / config.paths.vc_image_dir_path /
      task.save_dir;
  EnsureDirectory(output_dir.string());
  for (const auto& path : ListFiles(input_dir)) {
    const cv::Mat image = cv::imread(path.string(), cv::IMREAD_COLOR);
    if (image.empty()) {
      throw std::runtime_error("failed to read image: " + path.string());
    }
    cv::Mat remapped;
    cv::remap(image, remapped, maps.map_x, maps.map_y, cv::INTER_LINEAR);
    const std::string output_name = task.file_prefix + "_" + path.filename().string();
    if (!cv::imwrite((output_dir / output_name).string(), remapped)) {
      throw std::runtime_error("failed to write image: " +
                               (output_dir / output_name).string());
    }
  }

  LogInfo(config.showinfo != 0,
          BuildVirtualTaskDoneMessage(task, ElapsedMilliseconds(start_time)));
}
```

```cpp
void RunVirtualCameraPipeline(const PipelineConfig& config) {
  const auto start_time = std::chrono::steady_clock::now();
  LogInfo(config.showinfo != 0, BuildVirtualCameraPipelineStartMessage(config));

  if (config.virtual_camera_parallelism <= 1) {
    for (const auto& task : config.virtual_tasks) {
      RunVirtualCameraTask(config, task);
    }
    LogInfo(config.showinfo != 0,
            BuildVirtualCameraPipelineDoneMessage(ElapsedMilliseconds(start_time)));
    return;
  }

  ValidateVirtualCameraOutputs(config);

  std::vector<std::function<void()>> jobs;
  jobs.reserve(config.virtual_tasks.size());
  for (const auto& task : config.virtual_tasks) {
    jobs.push_back([&config, task]() { RunVirtualCameraTask(config, task); });
  }
  RunJobs(jobs, config.virtual_camera_parallelism);

  LogInfo(config.showinfo != 0,
          BuildVirtualCameraPipelineDoneMessage(ElapsedMilliseconds(start_time)));
}
```

在 `src/main.cpp` 调用入口日志：

```cpp
#include "virtual_camera/logging.h"
```

```cpp
    const vc::Rt024RuntimeArgs args = vc::ParseRt024RuntimeArgs(argc, argv);
    vc::PipelineConfig config = vc::LoadPipelineConfig(args.config_path);
    vc::ApplyRt024RuntimeArgs(args, &config);
    vc::LogInfo(config.showinfo != 0,
                vc::BuildRt024PipelineStartMessage(args, config));

    vc::RunTopLevelPipelines(
        config,
        [&config]() { vc::RunUndistortPipeline(config); },
        [&config]() { vc::RunVirtualCameraPipeline(config); });
```

在 `README.md` 补一小节：

```md
## 运行日志

`showinfo` 控制 RT024 运行日志：

- `showinfo = 1`：打印流水线和任务级日志
- `showinfo = 0`：只在结束或报错时打印

当前并行配置 `configs/config_rt024_parallel_run.json` 默认启用 `showinfo = 1`。
```

- [ ] **Step 4: Run the unit tests and verify they pass**

Run:

```bash
cmake --build build --target test_logging test_virtual_camera_processor virtual_camera_tool -j2
./build/test_logging
./build/test_virtual_camera_processor
```

Expected:

- 三个目标编译成功
- `./build/test_logging` 和 `./build/test_virtual_camera_processor` 都返回 `0`

- [ ] **Step 5: Run the real script command and verify the logs appear**

Run:

```bash
bash image_virtual.bash \
  --dataset_root /workspace/GACRT024_1754812994 \
  --config_path ./configs/config_rt024_parallel_run.json \
  --debug \
  --golden_root /workspace/GACRT024_1754812994 \
  --output_root /workspace/output_dataline_logging > /tmp/rt024_task_logging.log 2>&1

grep -E "pipeline start|undistort start|undistort task start|virtual_camera start|virtual task start|verification passed" /tmp/rt024_task_logging.log
```

Expected:

- `bash image_virtual.bash ...` 退出码为 `0`
- `grep` 能匹配到以下类型的行：
  - `[INFO] pipeline start: ...`
  - `[INFO] undistort start: ...`
  - `[INFO] undistort task start: ...`
  - `[INFO] virtual_camera start: ...`
  - `[INFO] virtual task start: ...`
  - `verification passed`

- [ ] **Step 6: Commit the runtime logging integration**

```bash
git add README.md src/main.cpp src/virtual_camera_processor.cpp tests/test_virtual_camera_processor.cpp
git commit -m "feat: add rt024 task-level progress logging"
```
