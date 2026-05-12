# Parallel Pipeline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 `process_undistort` 和 `process_virtual_camera` 增加可配置并行能力，支持主任务并行与任务内相机并行，同时保持默认行为和输出结果与当前串行版本一致。

**Architecture:** 复用现有 `include/virtual_camera/jobs.h` 中的并发基础设施，把并行控制收敛到配置解析、通用任务调度和两个 processor 的 task 分发层。单个 task 的图像处理、map 生成和 JSON 输出步骤保持不变，只调整调度方式与异常收口方式。

**Tech Stack:** C++17、nlohmann::json、std::thread、OpenCV、现有自定义轻量测试程序、catkin/cmake

---

## 文件结构

### 将修改

- `include/virtual_camera/pipeline_config.h`
  - 在 `PipelineConfig` 中新增三个并行度字段。
- `src/pipeline_config.cpp`
  - 解析三个新字段，默认值为 `1`。
- `include/virtual_camera/jobs.h`
  - 扩展现有并发工具，补一个按任意 job 列表执行的通用入口，复用现有异常收口逻辑。
- `src/main.cpp`
  - 把两个主任务改成可串行、可并行执行。
- `src/undistort_processor.cpp`
  - 把内部串行 task 循环改为按配置并发分发。
- `src/virtual_camera_processor.cpp`
  - 把内部串行 task 循环改为按配置并发分发。
- `tests/test_pipeline_config.cpp`
  - 补充新并行配置字段的默认值和显式值测试。
- `CMakeLists.txt`
  - 注册新增的测试可执行文件，不修改编译选项。

### 将新增

- `tests/test_parallel_executor.cpp`
  - 验证通用并发调度入口在串行/并行/异常场景下的行为。

---

### Task 1: 扩展配置结构与解析

**Files:**
- Modify: `include/virtual_camera/pipeline_config.h`
- Modify: `src/pipeline_config.cpp`
- Test: `tests/test_pipeline_config.cpp`

- [ ] **Step 1: 写配置解析的失败测试**

在 `tests/test_pipeline_config.cpp` 的成功用例 JSON 中加入显式并行字段，并新增一个默认值用例，验证缺省时三个字段都为 `1`：

```cpp
void TestLoadPipelineConfigParallelismDefaults() {
  const std::string root = "build/test_tmp/pipeline_config_parallel_defaults";
  std::filesystem::create_directories(root);
  const std::string path = root + "/config.json";
  WriteText(path, R"json({
    "dataset_root": "/workspace/GACRT024_1754812994",
    "golden_root": "/workspace/GACRT024_1754812994",
    "output_root": "build/rt024_output",
    "conf_dir_path": "calib_extract/",
    "image_dir_path": "image_raw/",
    "vc_image_dir_path": "image_virtual_camera/",
    "undistort_image_dir_path": "image_undistortion/",
    "vc_conf_dir_path": "calib_virtual_camera/",
    "undistort_conf_dir_path": "calib_undistortion/",
    "vc_gdcbin_dir_path": "vc_gdcbin_dir_path/",
    "virtual_camera_configs": [],
    "undistort_configs": []
  })json");

  const vc::PipelineConfig config = vc::LoadPipelineConfig(path);
  Expect(config.task_parallelism == 1, "default task_parallelism");
  Expect(config.undistort_parallelism == 1, "default undistort_parallelism");
  Expect(config.virtual_camera_parallelism == 1,
         "default virtual_camera_parallelism");
}
```

并把现有成功用例里的 JSON 增加：

```json
"task_parallelism": 2,
"undistort_parallelism": 3,
"virtual_camera_parallelism": 4,
```

并在断言里增加：

```cpp
Expect(config.task_parallelism == 2, "task_parallelism");
Expect(config.undistort_parallelism == 3, "undistort_parallelism");
Expect(config.virtual_camera_parallelism == 4, "virtual_camera_parallelism");
```

- [ ] **Step 2: 运行测试，确认当前失败**

Run:

```bash
cmake --build build --target test_pipeline_config && ./build/test_pipeline_config
```

Expected:

```text
FAIL，提示 `PipelineConfig` 缺少并行字段或断言失败
```

- [ ] **Step 3: 最小实现配置字段**

在 `include/virtual_camera/pipeline_config.h` 的 `PipelineConfig` 中新增字段：

```cpp
struct PipelineConfig {
  OutputPathConfig paths;
  int showinfo = 0;
  int showdir = 0;
  int process_virtual_camera = 0;
  int process_undistort = 0;
  int task_parallelism = 1;
  int undistort_parallelism = 1;
  int virtual_camera_parallelism = 1;
  int undistort_image = 0;
  int distort_model = 0;
  int save_virtual_json = 0;
  int save_undistort_json = 0;
  int conf_type = 0;
  std::string dataset_root;
  std::string golden_root;
  std::string output_root;
  std::vector<VirtualCameraTaskConfig> virtual_tasks;
  std::vector<UndistortTaskConfig> undistort_tasks;
};
```

在 `src/pipeline_config.cpp` 中解析默认值：

```cpp
  cfg.process_virtual_camera = json.value("process_virtual_camera", 0);
  cfg.process_undistort = json.value("process_undistort", 0);
  cfg.task_parallelism = json.value("task_parallelism", 1);
  cfg.undistort_parallelism = json.value("undistort_parallelism", 1);
  cfg.virtual_camera_parallelism =
      json.value("virtual_camera_parallelism", 1);
  cfg.undistort_image = json.value("undistort_image", 0);
```

- [ ] **Step 4: 重新运行配置测试**

Run:

```bash
cmake --build build --target test_pipeline_config && ./build/test_pipeline_config
```

Expected:

```text
PASS，程序退出码为 0
```

- [ ] **Step 5: 提交本任务**

```bash
git add include/virtual_camera/pipeline_config.h src/pipeline_config.cpp tests/test_pipeline_config.cpp
git commit -m "feat: add pipeline parallelism config"
```

---

### Task 2: 复用 jobs.h 提供通用任务调度入口

**Files:**
- Modify: `include/virtual_camera/jobs.h`
- Add Test: `tests/test_parallel_executor.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: 先写并发调度测试**

新增 `tests/test_parallel_executor.cpp`，覆盖三个场景：串行执行全部 job、并行执行全部 job、任一 job 异常时最终失败。

测试主体可以直接写成：

```cpp
#include "virtual_camera/jobs.h"

#include <atomic>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Expect(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void TestRunJobsSequential() {
  std::atomic<int> count{0};
  std::vector<std::function<void()>> jobs;
  for (int index = 0; index < 5; ++index) {
    jobs.push_back([&count]() { ++count; });
  }
  vc::RunJobs(jobs, 1);
  Expect(count.load() == 5, "sequential jobs");
}

void TestRunJobsParallel() {
  std::atomic<int> count{0};
  std::vector<std::function<void()>> jobs;
  for (int index = 0; index < 8; ++index) {
    jobs.push_back([&count]() { ++count; });
  }
  vc::RunJobs(jobs, 3);
  Expect(count.load() == 8, "parallel jobs");
}

void TestRunJobsPropagatesException() {
  std::vector<std::function<void()>> jobs;
  jobs.push_back([]() {});
  jobs.push_back([]() { throw std::runtime_error("boom"); });
  jobs.push_back([]() {});

  bool thrown = false;
  try {
    vc::RunJobs(jobs, 2);
  } catch (const std::runtime_error& error) {
    thrown = std::string(error.what()).find("boom") != std::string::npos;
  }
  Expect(thrown, "RunJobs should propagate exception");
}

}  // namespace

int main() {
  TestRunJobsSequential();
  TestRunJobsParallel();
  TestRunJobsPropagatesException();
  return 0;
}
```

同时在 `CMakeLists.txt` 注册测试目标：

```cmake
if(EXISTS ${PROJECT_SOURCE_DIR}/tests/test_parallel_executor.cpp)
    add_executable(test_parallel_executor tests/test_parallel_executor.cpp)
    target_link_libraries(test_parallel_executor PRIVATE virtual_camera_core)
endif()
```

- [ ] **Step 2: 运行测试，确认当前失败**

Run:

```bash
cmake --build build --target test_parallel_executor
```

Expected:

```text
FAIL，提示 `vc::RunJobs` 未定义
```

- [ ] **Step 3: 在 jobs.h 中实现通用入口**

在 `include/virtual_camera/jobs.h` 顶部增加头文件：

```cpp
#include <functional>
```

新增通用调度函数，并让 `ParallelFor` 复用它：

```cpp
inline void RunJobs(const std::vector<std::function<void()>>& jobs, int max_jobs) {
  if (jobs.empty()) {
    return;
  }

  if (max_jobs <= 1) {
    for (const auto& job : jobs) {
      job();
    }
    return;
  }

  const std::size_t worker_count =
      std::min<std::size_t>(jobs.size(),
                            static_cast<std::size_t>(std::max(1, max_jobs)));
  std::atomic<std::size_t> next{0};
  std::mutex error_mutex;
  std::vector<std::string> errors;
  std::vector<std::thread> workers;
  workers.reserve(worker_count);

  for (std::size_t worker_index = 0; worker_index < worker_count; ++worker_index) {
    workers.emplace_back([&]() {
      while (true) {
        const std::size_t index = next.fetch_add(1);
        if (index >= jobs.size()) {
          break;
        }
        try {
          jobs[index]();
        } catch (const std::exception& ex) {
          std::lock_guard<std::mutex> lock(error_mutex);
          errors.push_back(ex.what());
        } catch (...) {
          std::lock_guard<std::mutex> lock(error_mutex);
          errors.push_back("unknown exception");
        }
      }
    });
  }

  for (auto& worker : workers) {
    worker.join();
  }

  if (!errors.empty()) {
    throw std::runtime_error("parallel task failed: " + errors.front());
  }
}

template <typename Fn>
void ParallelFor(std::size_t count, int jobs, Fn fn) {
  std::vector<std::function<void()>> work_items;
  work_items.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    work_items.push_back([index, &fn]() { fn(index); });
  }
  RunJobs(work_items, jobs);
}
```

- [ ] **Step 4: 运行并发测试和现有 jobs 测试**

Run:

```bash
cmake --build build --target test_jobs test_parallel_executor
./build/test_jobs
./build/test_parallel_executor
```

Expected:

```text
两个测试都 PASS，退出码为 0
```

- [ ] **Step 5: 提交本任务**

```bash
git add include/virtual_camera/jobs.h tests/test_parallel_executor.cpp CMakeLists.txt
git commit -m "feat: add reusable pipeline job runner"
```

---

### Task 3: 为主流程增加主任务级并行调度

**Files:**
- Modify: `src/main.cpp`
- Test: `tests/test_parallel_executor.cpp`

- [ ] **Step 1: 先补主任务调度的单元测试**

在 `tests/test_parallel_executor.cpp` 中追加一个纯调度测试，模拟两个顶层任务并验证 `RunJobs` 可以承载“主任务列表”：

```cpp
void TestRunJobsSupportsTopLevelPipelines() {
  std::atomic<int> count{0};
  std::vector<std::function<void()>> jobs;
  jobs.push_back([&count]() { ++count; });
  jobs.push_back([&count]() { ++count; });
  vc::RunJobs(jobs, 2);
  Expect(count.load() == 2, "top level pipeline jobs");
}
```

这一步不会直接覆盖 `main()`，但会为主流程重构提供稳定调度契约。

- [ ] **Step 2: 运行测试，确认基线通过**

Run:

```bash
cmake --build build --target test_parallel_executor && ./build/test_parallel_executor
```

Expected:

```text
PASS，新增测试应通过
```

- [ ] **Step 3: 改造 main.cpp 为可并行的主任务调度**

把 `src/main.cpp` 的串行调用：

```cpp
    if (config.process_undistort != 0) {
      vc::RunUndistortPipeline(config);
    }
    if (config.process_virtual_camera != 0) {
      vc::RunVirtualCameraPipeline(config);
    }
```

改为基于 job 列表调度：

```cpp
#include "virtual_camera/jobs.h"
```

```cpp
    std::vector<std::function<void()>> pipeline_jobs;
    if (config.process_undistort != 0) {
      pipeline_jobs.push_back([&config]() { vc::RunUndistortPipeline(config); });
    }
    if (config.process_virtual_camera != 0) {
      pipeline_jobs.push_back(
          [&config]() { vc::RunVirtualCameraPipeline(config); });
    }

    vc::RunJobs(pipeline_jobs, config.task_parallelism);
```

保留后续校验逻辑不变：

```cpp
    const vc::VerifyResult result =
        vc::VerifyRt024Outputs(config.golden_root, config.output_root);
```

- [ ] **Step 4: 运行主流程相关回归测试**

Run:

```bash
cmake --build build --target virtual_camera_tool test_pipeline_config test_parallel_executor test_rt024_verifier
./build/test_pipeline_config
./build/test_parallel_executor
./build/test_rt024_verifier
```

Expected:

```text
全部 PASS，说明主流程改造未破坏配置解析和验证逻辑
```

- [ ] **Step 5: 提交本任务**

```bash
git add src/main.cpp tests/test_parallel_executor.cpp
git commit -m "feat: run top-level pipelines in parallel"
```

---

### Task 4: 为 undistort processor 增加任务内并行

**Files:**
- Modify: `src/undistort_processor.cpp`
- Test: `tests/test_parallel_executor.cpp`

- [ ] **Step 1: 先补基于索引调度的回归测试**

在 `tests/test_parallel_executor.cpp` 追加一个测试，验证用 `RunJobs` 承载多个独立图像 task 时不会丢任务：

```cpp
void TestRunJobsHandlesManyImageTasks() {
  std::atomic<int> count{0};
  std::vector<std::function<void()>> jobs;
  for (int index = 0; index < 16; ++index) {
    jobs.push_back([&count]() { ++count; });
  }
  vc::RunJobs(jobs, 4);
  Expect(count.load() == 16, "image jobs");
}
```

- [ ] **Step 2: 运行测试，确认基线通过**

Run:

```bash
cmake --build build --target test_parallel_executor && ./build/test_parallel_executor
```

Expected:

```text
PASS
```

- [ ] **Step 3: 把 undistort 的 task 循环改成并行分发**

在 `src/undistort_processor.cpp` 中增加：

```cpp
#include "virtual_camera/jobs.h"

#include <functional>
```

把原来的直接循环：

```cpp
  for (const auto& task : config.undistort_tasks) {
    CalibrationParam calibration = LoadUndistortSourceCalibration(config, task);
    ...
  }
```

改成先构造 job 列表，再调度：

```cpp
  std::vector<std::function<void()>> jobs;
  jobs.reserve(config.undistort_tasks.size());
  for (const auto& task : config.undistort_tasks) {
    jobs.push_back([&config, &task]() {
      CalibrationParam calibration =
          LoadUndistortSourceCalibration(config, task);

      double k_array[3][3];
      double d_array[8];
      double new_k_array[4];
      FillCameraMatrix(calibration.intrinsic_matrix, k_array);
      FillDistortion(calibration.dist_data, d_array);
      new_k_array[0] = task.new_intrinsic.focal_u;
      new_k_array[1] = task.new_intrinsic.center == 0
                           ? calibration.intrinsic_matrix(0, 2)
                           : task.new_intrinsic.center_u;
      new_k_array[2] = task.new_intrinsic.focal_v;
      new_k_array[3] = task.new_intrinsic.center == 0
                           ? calibration.intrinsic_matrix(1, 2)
                           : task.new_intrinsic.center_v;

      cv::Mat map_x;
      cv::Mat map_y;
      if (config.distort_model == 1) {
        gen_undis_map_kb(k_array, d_array, calibration.image_width,
                         calibration.image_height, new_k_array, map_x, map_y,
                         config.showinfo);
      } else {
        gen_undis_map(k_array, d_array, calibration.image_width,
                      calibration.image_height, new_k_array, map_x, map_y,
                      config.showinfo);
      }

      const std::filesystem::path json_output =
          std::filesystem::path(config.output_root) /
          config.paths.undistort_conf_dir_path / task.conf_json;
      WriteRt024UndistortJson(json_output.string(), calibration,
                              task.new_intrinsic);

      const std::filesystem::path input_dir =
          std::filesystem::path(config.dataset_root) /
          config.paths.image_dir_path / task.image_dir;
      const std::filesystem::path output_dir =
          std::filesystem::path(config.output_root) /
          config.paths.undistort_image_dir_path / task.image_dir;
      EnsureDirectory(output_dir.string());
      for (const auto& path : ListFiles(input_dir)) {
        const cv::Mat image = cv::imread(path.string(), cv::IMREAD_COLOR);
        if (image.empty()) {
          throw std::runtime_error("failed to read image: " + path.string());
        }
        cv::Mat remapped;
        cv::remap(image, remapped, map_x, map_y, cv::INTER_LINEAR);
        const std::filesystem::path output_path = output_dir / path.filename();
        if (!cv::imwrite(output_path.string(), remapped)) {
          throw std::runtime_error("failed to write image: " +
                                   output_path.string());
        }
      }
    });
  }

  RunJobs(jobs, config.undistort_parallelism);
```

注意这里必须继续保持：

- 单个 task 内的处理顺序不变
- 文件命名不变
- 每个 task 只写自己的输出目录和 JSON

- [ ] **Step 4: 运行相关测试**

Run:

```bash
cmake --build build --target test_parallel_executor test_rt024_json_writer test_rt024_verifier virtual_camera_tool
./build/test_parallel_executor
./build/test_rt024_json_writer
./build/test_rt024_verifier
```

Expected:

```text
全部 PASS
```

- [ ] **Step 5: 提交本任务**

```bash
git add src/undistort_processor.cpp tests/test_parallel_executor.cpp
git commit -m "feat: parallelize undistort tasks"
```

---

### Task 5: 为 virtual camera processor 增加任务内并行

**Files:**
- Modify: `src/virtual_camera_processor.cpp`
- Test: `tests/test_parallel_executor.cpp`

- [ ] **Step 1: 先补异常传播回归测试**

在 `tests/test_parallel_executor.cpp` 中增加“多 job 中间失败”的回归用例，确保 processor 级并发会把第一条异常带回主线程：

```cpp
void TestRunJobsReportsFirstFailure() {
  std::vector<std::function<void()>> jobs;
  jobs.push_back([]() { throw std::runtime_error("first"); });
  jobs.push_back([]() { throw std::runtime_error("second"); });

  bool thrown = false;
  try {
    vc::RunJobs(jobs, 2);
  } catch (const std::runtime_error& error) {
    const std::string message = error.what();
    thrown = message.find("parallel task failed") != std::string::npos &&
             message.find("first") != std::string::npos;
  }
  Expect(thrown, "RunJobs should report first failure");
}
```

- [ ] **Step 2: 运行测试，确认基线通过**

Run:

```bash
cmake --build build --target test_parallel_executor && ./build/test_parallel_executor
```

Expected:

```text
PASS
```

- [ ] **Step 3: 把 virtual camera 的 task 循环改成并行分发**

在 `src/virtual_camera_processor.cpp` 中增加：

```cpp
#include "virtual_camera/jobs.h"

#include <functional>
```

把原来的串行循环改为 job 列表调度：

```cpp
  std::vector<std::function<void()>> jobs;
  jobs.reserve(config.virtual_tasks.size());
  for (const auto& task : config.virtual_tasks) {
    jobs.push_back([&config, &task]() {
      CalibrationParam calibration = LoadVirtualSourceCalibration(config, task);

      double k_array[3][3];
      double d_array[8];
      double rt_array[4][4];
      FillCameraMatrix(calibration.intrinsic_matrix, k_array);
      FillDistortion(calibration.dist_data, d_array);
      FillExtrinsic(calibration.extrinsic_matrix, rt_array);

      cv::Mat map_x;
      cv::Mat map_y;
      cv::Mat src_map_x;
      cv::Mat src_map_y;
      if (config.distort_model == 1) {
        gen_vc_map_kb(k_array, d_array, rt_array,
                      task.new_intrinsic.image_width,
                      task.new_intrinsic.image_height,
                      task.new_intrinsic.fov, task.new_extrinsics.yaw, map_x,
                      map_y, config.showinfo, 1, task.image_width,
                      task.image_height, src_map_x, src_map_y);
      } else {
        gen_vc_map(k_array, d_array, rt_array,
                   task.new_intrinsic.image_width,
                   task.new_intrinsic.image_height, task.new_intrinsic.fov,
                   task.new_extrinsics.yaw, map_x, map_y, config.showinfo, 1,
                   task.image_width, task.image_height, src_map_x, src_map_y);
      }

      const std::filesystem::path map_root =
          std::filesystem::path(config.output_root) /
          config.paths.vc_gdcbin_dir_path;
      SaveFloatMap(map_root / task.vc_mapx_name, map_x);
      SaveFloatMap(map_root / task.vc_mapy_name, map_y);
      SaveFloatMap(map_root / task.src2vc_mapx_name, src_map_x);
      SaveFloatMap(map_root / task.src2vc_mapy_name, src_map_y);

      VirtualParam virtual_param = BuildVirtualParam(task, calibration);
      MapGenerator generator(calibration, virtual_param);
      const std::filesystem::path json_output =
          std::filesystem::path(config.output_root) /
          config.paths.vc_conf_dir_path / task.calib_json;
      WriteRt024VirtualJson(json_output.string(), generator.virtual_intrinsic(),
                            generator.virtual_extrinsic(),
                            std::vector<double>(8, 0.0));

      const std::filesystem::path input_dir =
          std::filesystem::path(config.dataset_root) /
          config.paths.image_dir_path / task.image_dir;
      const std::filesystem::path output_dir =
          std::filesystem::path(config.output_root) /
          config.paths.vc_image_dir_path / task.save_dir;
      EnsureDirectory(output_dir.string());
      for (const auto& path : ListFiles(input_dir)) {
        const cv::Mat image = cv::imread(path.string(), cv::IMREAD_COLOR);
        if (image.empty()) {
          throw std::runtime_error("failed to read image: " + path.string());
        }
        cv::Mat remapped;
        cv::remap(image, remapped, map_x, map_y, cv::INTER_LINEAR);
        const std::string output_name =
            task.file_prefix + "_" + path.filename().string();
        const std::filesystem::path output_path = output_dir / output_name;
        if (!cv::imwrite(output_path.string(), remapped)) {
          throw std::runtime_error("failed to write image: " +
                                   output_path.string());
        }
      }
    });
  }

  RunJobs(jobs, config.virtual_camera_parallelism);
```

- [ ] **Step 4: 运行相关测试**

Run:

```bash
cmake --build build --target test_parallel_executor test_rt024_json_writer test_rt024_verifier virtual_camera_tool
./build/test_parallel_executor
./build/test_rt024_json_writer
./build/test_rt024_verifier
```

Expected:

```text
全部 PASS
```

- [ ] **Step 5: 提交本任务**

```bash
git add src/virtual_camera_processor.cpp tests/test_parallel_executor.cpp
git commit -m "feat: parallelize virtual camera tasks"
```

---

### Task 6: 全量回归与手工串并行比对

**Files:**
- Modify: `configs/config_rt024.json`
  - 仅在本地临时验证时编辑并行参数；不要提交调参结果，除非明确需要提交示例配置。
- Verify: `docs/superpowers/specs/2026-05-12-parallel-pipeline-design.md`

- [ ] **Step 1: 运行全部已有单元测试**

Run:

```bash
cmake --build build --target \
  test_jobs \
  test_pipeline_config \
  test_parallel_executor \
  test_calibration_loader \
  test_rt024_json_writer \
  test_rt024_verifier
./build/test_jobs
./build/test_pipeline_config
./build/test_parallel_executor
./build/test_calibration_loader
./build/test_rt024_json_writer
./build/test_rt024_verifier
```

Expected:

```text
全部 PASS
```

- [ ] **Step 2: 用串行配置跑一版基线结果**

把 `configs/config_rt024.json` 中三个字段设为：

```json
"task_parallelism": 1,
"undistort_parallelism": 1,
"virtual_camera_parallelism": 1
```

Run:

```bash
./build/virtual_camera_tool configs/config_rt024.json
```

Expected:

```text
输出 verifier 成功信息，退出码为 0
```

- [ ] **Step 3: 用并行配置跑一版结果**

临时改为：

```json
"task_parallelism": 2,
"undistort_parallelism": 2,
"virtual_camera_parallelism": 2
```

Run:

```bash
./build/virtual_camera_tool configs/config_rt024.json
```

Expected:

```text
输出 verifier 成功信息，退出码为 0
```

- [ ] **Step 4: 比对串并行结果**

如果输出目录分开保存，执行目录对比：

```bash
diff -ruN build/rt024_output_serial build/rt024_output_parallel
```

或至少确认 verifier 两次都通过，并记录关键目录一致：

```bash
find build/rt024_output_serial -type f | sort > build/serial_files.txt
find build/rt024_output_parallel -type f | sort > build/parallel_files.txt
diff -u build/serial_files.txt build/parallel_files.txt
```

Expected:

```text
文件集合一致，或 verifier 全通过且无新增差异
```

- [ ] **Step 5: 提交最终实现**

```bash
git add include/virtual_camera/pipeline_config.h \
        src/pipeline_config.cpp \
        include/virtual_camera/jobs.h \
        src/main.cpp \
        src/undistort_processor.cpp \
        src/virtual_camera_processor.cpp \
        tests/test_pipeline_config.cpp \
        tests/test_parallel_executor.cpp \
        CMakeLists.txt
git commit -m "feat: add configurable pipeline parallelism"
```

---

## 自检

- Spec coverage
  - 主任务并行：Task 3 覆盖
  - `undistort` 内部并行：Task 4 覆盖
  - `virtual camera` 内部并行：Task 5 覆盖
  - 配置项与默认值：Task 1 覆盖
  - 结果一致性与异常收口：Task 2、Task 4、Task 5、Task 6 覆盖
  - 测试与回归：Task 1、Task 2、Task 6 覆盖

- Placeholder scan
  - 无 `TODO`、`TBD`、`implement later` 之类占位语

- Type consistency
  - 配置字段统一使用 `task_parallelism`、`undistort_parallelism`、`virtual_camera_parallelism`
  - 并发入口统一使用 `vc::RunJobs`

