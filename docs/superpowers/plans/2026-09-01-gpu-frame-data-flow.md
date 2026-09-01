# GPU Frame Data Flow Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将图片读取、GPU 上传、去畸变/虚拟相机处理和结果保存拆开，使输入与输出在处理阶段保持 GPU 图片数据，保存时才下载到 CPU。

**Architecture:** 在 CUDA remap 模块中定义 `GpuImage` 包装类型和路径读取函数。两个处理器接收 `GpuImage`，通过 `cv::cuda::remap` 直接生成 GPU 结果；保存函数负责下载结果并调用现有 `cv::imwrite`。Python 绑定暴露 `GpuImage` 和 `read_image()`，处理函数接收该对象，结果仍通过 result ID 保存。

**Tech Stack:** C++17、OpenCV CUDA (`cv::cuda::GpuMat`)、pybind11、Python 3、现有手写 C++ 测试、Python workflow 测试。

---

## 文件变更地图

- Create: `docs/superpowers/plans/2026-09-01-gpu-frame-data-flow.md`，记录本实现计划。
- Modify: `include/virtual_camera/cuda_remap.h`，定义 GPU 图片类型、图片读取 API 和 GPU 输入 remap API。
- Modify: `src/cuda_remap.cpp`，实现路径读取、GPU 输入校验、GPU remap 和 CPU 下载辅助逻辑。
- Modify: `include/virtual_camera/undistort_processor.h`，让去畸变结果保存 GPU 图片。
- Modify: `src/undistort_processor.cpp`，批处理读取阶段上传一次，处理阶段直接复用 GPU 图片，保存阶段下载。
- Modify: `include/virtual_camera/virtual_camera_processor.h`，让虚拟相机结果保存 GPU 图片。
- Modify: `src/virtual_camera_processor.cpp`，批处理读取阶段上传一次，处理阶段直接复用 GPU 图片，保存阶段下载。
- Modify: `include/virtual_camera/pipeline_orchestrator.h`，暴露 C++ `ReadImage` 和 GPU 输入处理签名。
- Modify: `src/pipeline_orchestrator.cpp`，转发 GPU 图片读取和处理。
- Modify: `src/main.cpp`，命令行批处理改用统一 GPU 读取函数。
- Modify: `src/python_bindings.cpp`，绑定 `GpuImage` 和新的 Python 两步接口，保留便捷接口。
- Modify: `tests/test_cuda_remap.cpp`，增加读取、GPU 输入 remap 和下载验证。
- Modify: `tests/test_undistort_processor.cpp`，迁移处理调用并验证 GPU 结果。
- Modify: `tests/test_virtual_camera_processor.cpp`，迁移处理调用并验证 GPU 结果。
- Modify: `tests/test_pipeline_orchestrator.cpp`，迁移 orchestrator 调用并验证共享 GPU 输入。
- Modify: `tests/test_python_pipeline_orchestrator.py`，验证 Python 新接口和便捷接口。
- Modify: `tests/benchmark_pipeline_timing.cpp`，将读取阶段计入 GPU 上传并传递 GPU 图片。
- Modify: `tests/benchmark_pipeline_timing_parallel.cpp`，将读取阶段计入 GPU 上传并传递 GPU 图片。
- Modify: `README.md`，更新 Python 分离式接口示例和数据类型说明。

### Task 1: 建立 GPU 图片和 remap API

**Files:**
- Modify: `include/virtual_camera/cuda_remap.h`
- Modify: `src/cuda_remap.cpp`
- Test: `tests/test_cuda_remap.cpp`

- [ ] **Step 1: 写失败测试，固定 GPU 图片 API 的行为**

在 `tests/test_cuda_remap.cpp` 增加测试辅助函数和以下测试：

```cpp
void TestReadImageUploadsToGpu() {
  const std::filesystem::path image_path = MakeSyntheticImagePath();
  const vc::GpuImage image = vc::ReadImage(image_path.string());

  Expect(!image.image.empty(), "read image should upload a non-empty GPU image");
  Expect(image.image.rows == 4 && image.image.cols == 4,
         "GPU image should keep source dimensions");
}

void TestGpuRemapKeepsOutputOnGpu() {
  cv::Mat source = MakeIdentityTestImage();
  const vc::CudaRemapMaps maps = vc::UploadCudaRemapMaps(
      MakeIdentityMapX(source.size()), MakeIdentityMapY(source.size()));

  cv::cuda::GpuMat gpu_source;
  gpu_source.upload(source);
  const cv::cuda::GpuMat gpu_output = vc::GpuRemap(gpu_source, maps);

  Expect(!gpu_output.empty(), "GPU remap output should stay on GPU");
  cv::Mat output;
  gpu_output.download(output);
  Expect(output.size() == source.size(),
         "GPU remap output should keep image size");
  Expect(output.at<cv::Vec3b>(0, 0) == source.at<cv::Vec3b>(0, 0),
         "GPU remap output should preserve identity pixels");
}

void TestReadImageRejectsMissingFile() {
  bool thrown = false;
  try {
    vc::ReadImage("build/test_tmp/missing-image.jpg");
  } catch (const std::runtime_error& error) {
    thrown = std::string(error.what()).find("failed to read image") !=
             std::string::npos;
  }
  Expect(thrown, "missing image should throw a read error");
}
```

复用现有 CUDA 前置检查和临时目录模式；测试只在 CUDA 可用环境执行。

- [ ] **Step 2: 运行测试确认 API 尚未实现**

Run:

```bash
cmake --build build --target test_cuda_remap -j2
```

Expected: 编译失败，错误包含 `GpuImage` 未定义或 `GpuRemap` 没有
`cv::cuda::GpuMat` 重载。

- [ ] **Step 3: 实现 GPU 图片类型和读取/remap API**

在 `include/virtual_camera/cuda_remap.h` 增加：

```cpp
struct GpuImage {
  cv::cuda::GpuMat image;
};

GpuImage ReadImage(const std::string& image_path);
cv::cuda::GpuMat GpuRemap(const cv::cuda::GpuMat& image,
                          const CudaRemapMaps& maps);
```

保留现有 CPU 输入 `GpuRemap(const cv::Mat&, ...)`，让命令行和旧测试可以在迁移
期间逐步切换；最终所有逐帧生产调用使用 GPU 重载。新增头文件依赖
`<string>`。

在 `src/cuda_remap.cpp` 中：

```cpp
GpuImage ReadImage(const std::string& image_path) {
  RequireCudaRemapAvailable();
  const cv::Mat cpu_image = cv::imread(image_path, cv::IMREAD_COLOR);
  if (cpu_image.empty()) {
    throw std::runtime_error("failed to read image: " + image_path);
  }

  GpuImage image;
  image.image.upload(cpu_image);
  return image;
}

cv::cuda::GpuMat GpuRemap(const cv::cuda::GpuMat& image,
                          const CudaRemapMaps& maps) {
  RequireCudaRemapAvailable();
  if (image.empty()) {
    throw std::runtime_error("CUDA remap image must not be empty");
  }
  if (maps.map_x.empty() || maps.map_y.empty()) {
    throw std::runtime_error("CUDA remap maps must be uploaded before remap");
  }

  cv::cuda::GpuMat output;
  cv::cuda::remap(image, output, maps.map_x, maps.map_y, cv::INTER_LINEAR);
  return output;
}
```

原有 CPU 重载改为上传后调用 GPU 重载，再下载结果，避免重复实现 remap：

```cpp
cv::Mat GpuRemap(const cv::Mat& image, const CudaRemapMaps& maps) {
  if (image.empty()) {
    throw std::runtime_error("CUDA remap image must not be empty");
  }
  cv::cuda::GpuMat gpu_input;
  gpu_input.upload(image);
  cv::cuda::GpuMat gpu_output = GpuRemap(gpu_input, maps);
  cv::Mat output;
  gpu_output.download(output);
  return output;
}
```

添加必要的 `opencv2/imgcodecs.hpp`、`stdexcept` 和 `<string>` include；代码注释
使用中文且只保留非显而易见的说明。

- [ ] **Step 4: 运行 CUDA remap 测试**

Run:

```bash
cmake --build build --target test_cuda_remap -j2
./build/test_cuda_remap
```

Expected: 测试通过；若环境无 CUDA，测试应明确报告现有 CUDA 可用性异常。

- [ ] **Step 5: 提交基础 GPU 图片 API**

```bash
git add include/virtual_camera/cuda_remap.h src/cuda_remap.cpp \
  tests/test_cuda_remap.cpp
git commit -m "feat: add GPU image read and remap APIs"
```

### Task 2: 迁移两个处理器到 GPU 输入和 GPU 输出

**Files:**
- Modify: `include/virtual_camera/undistort_processor.h`
- Modify: `src/undistort_processor.cpp`
- Modify: `include/virtual_camera/virtual_camera_processor.h`
- Modify: `src/virtual_camera_processor.cpp`
- Test: `tests/test_undistort_processor.cpp`
- Test: `tests/test_virtual_camera_processor.cpp`

- [ ] **Step 1: 先迁移测试输入，固定结果为 GPU 图片**

将两个测试文件中直接传入 `cv::Mat` 的处理测试改成：

```cpp
cv::cuda::GpuMat gpu_image;
gpu_image.upload(image);

const std::vector<vc::UndistortFrameResult> results =
    vc::ProcessUndistortFrame(cache, 1, vc::GpuImage{gpu_image});
```

虚拟相机测试使用相同形式。断言增加：

```cpp
Expect(!results.front().image.image.empty(),
       "processed image should remain on GPU");
```

缺失 camera ID 测试也传入同一个非空 `GpuImage`，保持“返回空结果”的行为。

- [ ] **Step 2: 运行处理器测试确认签名不匹配**

Run:

```bash
cmake --build build --target test_undistort_processor test_virtual_camera_processor -j2
```

Expected: 编译失败，错误显示处理器仍只接受 `cv::Mat` 或结果图片仍是
`cv::Mat`。

- [ ] **Step 3: 修改结果结构和处理函数签名**

在两个处理器头文件中：

```cpp
struct UndistortFrameResult {
  UndistortTaskConfig task;
  GpuImage image;
};

std::vector<UndistortFrameResult> ProcessUndistortFrame(
    const UndistortCache& cache, int camera_id, const GpuImage& image);
```

虚拟相机使用对应的 `VirtualCameraFrameResult` 和函数签名。头文件通过
`cuda_remap.h` 获得 `GpuImage` 定义。

- [ ] **Step 4: 让处理器直接调用 GPU remap**

将处理体中的：

```cpp
result.image = GpuRemap(image, entry.gpu_maps);
```

改成：

```cpp
result.image.image = GpuRemap(image.image, entry.gpu_maps);
```

在处理入口保留 camera ID 查找逻辑，并在查找前校验：

```cpp
if (image.image.empty()) {
  throw std::runtime_error("GPU image must not be empty");
}
```

保存函数先下载：

```cpp
cv::Mat cpu_image;
if (result.image.image.empty()) {
  throw std::runtime_error("GPU result image must not be empty");
}
result.image.image.download(cpu_image);
if (!cv::imwrite(output_path, cpu_image)) {
  throw std::runtime_error("failed to write image: " + output_path);
}
```

保存路径和文件名沿用现有逻辑，不改输出目录结构。

- [ ] **Step 5: 运行处理器测试确认 GPU 输入输出**

Run:

```bash
cmake --build build --target test_undistort_processor test_virtual_camera_processor -j2
./build/test_undistort_processor
./build/test_virtual_camera_processor
```

Expected: 测试通过，cache 中 GPU 映射表非空，处理结果 GPU 图片非空，保存回归
测试继续生成原有输出。

- [ ] **Step 6: 提交处理器迁移**

```bash
git add include/virtual_camera/undistort_processor.h \
  src/undistort_processor.cpp include/virtual_camera/virtual_camera_processor.h \
  src/virtual_camera_processor.cpp tests/test_undistort_processor.cpp \
  tests/test_virtual_camera_processor.cpp
git commit -m "feat: keep frame processing on GPU"
```

### Task 3: 迁移 orchestrator、命令行 pipeline 和 benchmark

**Files:**
- Modify: `include/virtual_camera/pipeline_orchestrator.h`
- Modify: `src/pipeline_orchestrator.cpp`
- Modify: `src/undistort_processor.cpp`
- Modify: `src/virtual_camera_processor.cpp`
- Modify: `src/main.cpp`
- Modify: `tests/benchmark_pipeline_timing.cpp`
- Modify: `tests/benchmark_pipeline_timing_parallel.cpp`
- Test: `tests/test_pipeline_orchestrator.cpp`

- [ ] **Step 1: 更新 orchestrator 测试为共享 GPU 输入**

在 `tests/test_pipeline_orchestrator.cpp` 中，将 `cv::imread` 后的 CPU 图片转换为：

```cpp
const vc::GpuImage image = vc::ReadImage(source_image.string());
```

随后调用：

```cpp
const auto undistort_results =
    orchestrator.ProcessUndistortFrame(1, image);
const auto virtual_results =
    orchestrator.ProcessVirtualCameraFrame(1, image);
```

断言两个结果的 `.image.image` 非空，并保持处理阶段不产生输出文件、保存阶段
才产生文件的既有断言。

- [ ] **Step 2: 运行 orchestrator 测试确认接口尚未迁移**

Run:

```bash
cmake --build build --target test_pipeline_orchestrator -j2
```

Expected: 编译失败，错误显示 orchestrator 方法仍接受 `cv::Mat`。

- [ ] **Step 3: 修改 orchestrator C++ 接口**

在 `include/virtual_camera/pipeline_orchestrator.h` 增加：

```cpp
GpuImage ReadImage(const std::string& image_path) const;
```

并将两个处理接口改为：

```cpp
std::vector<UndistortFrameResult> ProcessUndistortFrame(
    int camera_id, const GpuImage& image) const;
std::vector<VirtualCameraFrameResult> ProcessVirtualCameraFrame(
    int camera_id, const GpuImage& image) const;
```

在 `src/pipeline_orchestrator.cpp` 中直接转发 `vc::ReadImage()` 和两个处理器
函数，不在 orchestrator 内重新读取或上传。

- [ ] **Step 4: 更新命令行批处理读取和处理顺序**

在 `RunUndistortPipeline` 和 `RunVirtualCameraPipeline` 中，将：

```cpp
const cv::Mat image = cv::imread(path.string(), cv::IMREAD_COLOR);
```

替换为：

```cpp
const GpuImage image = ReadImage(path.string());
```

并把处理调用传入 `image`。保存函数负责将每个 GPU 结果下载后写盘。这样批处理
每个输入文件只执行一次图片上传。

`src/main.cpp` 中同样使用 `ReadImage()`，保持输入文件枚举、处理和保存顺序。

- [ ] **Step 5: 更新两个 benchmark 的阶段计时**

在 benchmark 的读取阶段把局部变量从 `cv::Mat` 改为 `vc::GpuImage`，读取调用改为
`vc::ReadImage(path.string())`；处理调用继续把该对象传给对应处理函数。读取时间
现在包含磁盘读取和一次 CPU 到 GPU 上传，处理时间只统计 GPU remap，保存时间
包含 GPU 到 CPU 下载和 `imwrite`。

- [ ] **Step 6: 运行 C++ orchestrator、pipeline 和 benchmark 编译验证**

Run:

```bash
cmake --build build --target test_pipeline_orchestrator \
  benchmark_pipeline_timing benchmark_pipeline_timing_parallel \
  virtual_camera_tool -j2
./build/test_pipeline_orchestrator
```

Expected: 目标编译成功，orchestrator 测试通过；benchmark 可执行文件成功生成。

- [ ] **Step 7: 提交 C++ 调用方迁移**

```bash
git add include/virtual_camera/pipeline_orchestrator.h \
  src/pipeline_orchestrator.cpp src/main.cpp \
  src/undistort_processor.cpp src/virtual_camera_processor.cpp \
  tests/test_pipeline_orchestrator.cpp tests/benchmark_pipeline_timing.cpp \
  tests/benchmark_pipeline_timing_parallel.cpp
git commit -m "refactor: separate GPU image loading from processing"
```

### Task 4: 更新 Python binding 和 Python workflow

**Files:**
- Modify: `src/python_bindings.cpp`
- Modify: `tests/test_python_pipeline_orchestrator.py`
- Modify: `README.md`

- [ ] **Step 1: 更新 Python 测试，先固定新接口**

在 `tests/test_python_pipeline_orchestrator.py` 中，将路径直接传给处理函数的代码改为：

```python
image = orchestrator.read_image(str(source_image))
virtual_result_id = orchestrator.process_virtual_camera_frame(1, image)
undistort_result_id = orchestrator.process_undistort_frame(1, image)
```

增加类型断言：

```python
expect(type(image).__name__ == "GpuImage",
       "read_image should return a GpuImage")
```

保留现有“处理阶段没有输出、保存阶段生成输出”的断言，并增加便捷接口验证：

```python
orchestrator.process_and_save_virtual_camera_frame(
    1, str(source_image), str(root / "convenience_virtual")
)
orchestrator.process_and_save_undistort_frame(
    1, str(source_image), str(root / "convenience_undistort")
)
```

验证对应输出目录中生成图片。

- [ ] **Step 2: 运行 Python 测试确认 binding 尚未迁移**

Run:

```bash
python3 tests/test_python_pipeline_orchestrator.py
```

Expected: 失败，提示 `PipelineOrchestrator` 没有 `read_image`，或处理函数无法接收
GPU 图片对象。

- [ ] **Step 3: 绑定 `GpuImage` 和新的读取/处理接口**

在 `src/python_bindings.cpp` 中：

1. 在模块初始化前定义：

```cpp
py::class_<vc::GpuImage>(module, "GpuImage");
```

2. 在 `PyPipelineOrchestrator` 中增加：

```cpp
vc::GpuImage ReadImage(const std::string& image_path) const {
  return vc::ReadImage(image_path);
}
```

3. 将处理方法改为接收 `const vc::GpuImage& image`：

```cpp
int ProcessVirtualCameraFrame(int camera_id,
                              const vc::GpuImage& image) const;
int ProcessUndistortFrame(int camera_id,
                          const vc::GpuImage& image) const;
```

4. 结果缓存继续保存 `vector<...FrameResult>`，保存时调用 orchestrator 的 GPU
   结果保存接口。

5. 便捷接口内部改为：

```cpp
const vc::GpuImage image = vc::ReadImage(image_path);
const int result_id = ProcessVirtualCameraFrame(camera_id, image);
SaveVirtualCameraFrameResults(
    result_id, output_root, std::filesystem::path(image_path).filename().string());
```

去畸变便捷接口使用相同结构。

在模块初始化中增加：

```cpp
.def("read_image", &PyPipelineOrchestrator::ReadImage,
     py::arg("image_path"))
```

并将两个 `.def("process_*_frame", ...)` 的参数名改为 `image`。

- [ ] **Step 4: 更新 README Python 示例**

将所有分离式调用从：

```python
orchestrator.process_virtual_camera_frame(1, str(image_path))
```

改为在循环中先执行：

```python
image = orchestrator.read_image(str(image_path))
result_id = orchestrator.process_virtual_camera_frame(
    source["camera_id"], image
)
```

在接口说明中明确：`GpuImage` 只由 `read_image()` 创建，处理结果保存时才下载
到 CPU；第一个整数参数是源相机 ID。

- [ ] **Step 5: 构建 Python 模块并运行 workflow 测试**

Run:

```bash
cmake --build build --target virtual_camera -j2
python3 tests/test_python_pipeline_orchestrator.py
```

Expected: Python workflow 通过，两个处理流程复用同一个 `GpuImage`，处理阶段不
生成图片，调用保存后生成原有命名的图片；便捷接口也通过。

- [ ] **Step 6: 提交 Python 接口迁移**

```bash
git add src/python_bindings.cpp tests/test_python_pipeline_orchestrator.py README.md
git commit -m "feat: expose GPU image workflow to Python"
```

### Task 5: 完整回归和接口一致性检查

**Files:**
- Modify: 由测试或编译检查发现的上述实现文件

- [ ] **Step 1: 检查遗留的 CPU 逐帧调用**

Run:

```bash
grep -RIn --exclude-dir=.git --exclude-dir=build --exclude-dir=.worktrees \
  "ProcessUndistortFrame\\|ProcessVirtualCameraFrame" include src tests
```

Expected: 所有调用的第三个参数都是 `GpuImage`，只有保留的
`GpuRemap(const cv::Mat&, ...)` 兼容重载仍接收 CPU 图片。

- [ ] **Step 2: 检查处理函数中不存在输入上传或结果下载**

Run:

```bash
grep -RIn --exclude-dir=.git --exclude-dir=build --exclude-dir=.worktrees \
  "upload\\|download" src/undistort_processor.cpp \
  src/virtual_camera_processor.cpp
```

Expected: 两个处理器逐帧函数不出现 `upload`/`download`；上传只在
`ReadImage()`，下载只在 `Save*FrameResult()`。

- [ ] **Step 3: 构建全部目标**

Run:

```bash
cmake --build build -j2
```

Expected: 所有 C++ target 和 Python module 编译成功。

- [ ] **Step 4: 运行核心 C++ 测试**

Run:

```bash
./build/test_cuda_remap
./build/test_undistort_processor
./build/test_virtual_camera_processor
./build/test_pipeline_orchestrator
./build/test_remap_generator
./build/test_pipeline_config
```

Expected: 所有测试返回 `0`。

- [ ] **Step 5: 运行 Python 测试**

Run:

```bash
python3 tests/test_python_pipeline_orchestrator.py
```

Expected: 返回 `0`，且测试临时目录中同时存在虚拟相机和去畸变输出图片。

- [ ] **Step 6: 检查工作区和提交内容**

Run:

```bash
git status --short
git log -6 --oneline
git diff HEAD~5..HEAD --check
```

Expected: 没有由本任务产生的未提交源码修改，最近提交包含 GPU API、处理器迁移、
C++ 调用方迁移和 Python 接口迁移；`diff --check` 无输出。
