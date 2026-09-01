# GPU 逐帧图片数据流设计

## 背景

当前项目的 C++ 处理函数已经接收 CPU 侧 `cv::Mat`，并在
`GpuRemap()` 内部执行以下流程：

```text
cv::Mat -> cv::cuda::GpuMat.upload()
         -> cv::cuda::remap()
         -> download()
         -> cv::Mat
```

Python 绑定仍然接收图片路径，并在
`process_virtual_camera_frame()` 和 `process_undistort_frame()` 内部读取图片。
这使得同一张输入图片不能被两个处理流程复用，也使读取、上传和处理职责混在
一起。

## 目标

将去畸变和虚拟相机的逐帧流程改为 GPU 图片数据流：

1. 独立读取图片路径并上传为 GPU 图片。
2. 处理函数直接接收 GPU 图片。
3. 处理结果保持 GPU 图片格式。
4. 保存函数被调用时才将结果下载为 CPU `cv::Mat` 并写盘。
5. 同一张 GPU 图片可以同时传给去畸变和虚拟相机流程。
6. 保留现有的便捷处理并保存接口。

## 非目标

- 不改变配置文件字段。
- 不改变标定加载和映射表生成算法。
- 不改变输出目录结构和文件命名规则。
- 不实现 CPU remap fallback。
- 不调整 CMake 编译选项。
- 不要求 Python 直接构造或操作 `cv::cuda::GpuMat`。

## 方案

采用 C++ GPU 图片包装类型，并通过 pybind11 暴露为 Python 的 `GpuImage`
对象。该对象由 C++ 持有 `cv::cuda::GpuMat`，Python 只负责传递对象。

```cpp
struct GpuImage {
  cv::cuda::GpuMat image;
};
```

结果结构中的图片字段也改为 GPU 图片类型：

```cpp
struct UndistortFrameResult {
  UndistortTaskConfig task;
  GpuImage image;
};

struct VirtualCameraFrameResult {
  VirtualCameraTaskConfig task;
  GpuImage image;
};
```

处理接口调整为接收 GPU 图片：

```cpp
std::vector<UndistortFrameResult> ProcessUndistortFrame(
    const UndistortCache& cache, int camera_id, const GpuImage& image);

std::vector<VirtualCameraFrameResult> ProcessVirtualCameraFrame(
    const VirtualCameraCache& cache, int camera_id, const GpuImage& image);
```

在处理器内部，GPU remap 直接使用 `cv::cuda::GpuMat`，不再对输入图片执行
第二次上传，输出也不下载：

```cpp
cv::cuda::GpuMat GpuRemap(const cv::cuda::GpuMat& image,
                          const CudaRemapMaps& maps);
```

## 接口与数据流

### C++ 层

新增读取函数：

```cpp
GpuImage ReadImage(const std::string& image_path);
```

其职责是：

1. 检查 CUDA 设备可用。
2. 使用 `cv::imread(image_path, cv::IMREAD_COLOR)` 读取 CPU 图片。
3. 检查读取结果非空。
4. 将图片上传到 `cv::cuda::GpuMat`。
5. 返回 GPU 图片对象。

保存函数继续接收处理结果和文件名，但在保存阶段执行：

```text
GpuImage
  -> cv::cuda::GpuMat.download()
  -> cv::Mat
  -> cv::imwrite()
```

缓存中的 remap 映射表仍在构建阶段上传一次，并继续保留 CPU 映射表用于现有
bin 输出和相关测试。

### Python 层

Python 接口改为：

```python
image = orchestrator.read_image("/path/source.jpg")

virtual_result_id = orchestrator.process_virtual_camera_frame(1, image)
undistort_result_id = orchestrator.process_undistort_frame(1, image)

orchestrator.save_virtual_camera_frame_results(
    virtual_result_id, output_root, "source.jpg"
)
orchestrator.save_undistort_frame_results(
    undistort_result_id, output_root, "source.jpg"
)
```

其中整数参数 `1` 是源相机 ID，用于选择对应的缓存映射表；返回的
`virtual_result_id` 和 `undistort_result_id` 才是结果 ID。

Python 绑定将：

- 暴露 `GpuImage` 类型。
- 暴露 `PipelineOrchestrator.read_image(image_path)`。
- 将两个 `process_*_frame` 方法改为接收 `GpuImage`。
- 保留 `save_*_frame_results` 的结果 ID 机制。
- 保留 `process_and_save_*_frame`，其内部按“读取、处理、保存”顺序执行。

### 完整数据流

```text
图片路径
  -> read_image()
  -> CPU cv::Mat
  -> 一次 upload()
  -> GpuImage(cv::cuda::GpuMat)
       ├─> process_virtual_camera_frame()
       │     -> GPU remap
       │     -> GpuImage 结果
       └─> process_undistort_frame()
             -> GPU remap
             -> GpuImage 结果

GpuImage 结果
  -> save_*_frame_results()
  -> download()
  -> CPU cv::Mat
  -> imwrite()
```

## 错误处理

- `ReadImage()` 在没有 CUDA 设备时抛出现有 CUDA 可用性异常。
- 图片读取失败或图片为空时抛出包含输入路径的 `std::runtime_error`。
- `GpuImage` 为空时，处理函数抛出明确异常。
- 不存在对应 `camera_id` 时保持当前行为，返回空结果列表。
- 保存空结果图片时抛出明确异常。
- GPU 下载或图片写盘失败时抛出异常。
- 处理和保存阶段不增加 CPU fallback。

## 测试

### C++ 测试

调整现有 GPU remap 和 orchestrator 测试，覆盖：

- 图片读取后得到非空 GPU 图片。
- GPU 输入 remap 重载不重复上传输入。
- GPU 输出保持非空、尺寸正确，并可在保存时下载。
- 同一个 GPU 图片可以同时传给虚拟相机和去畸变处理。
- 保存函数能生成现有路径和命名规则下的输出文件。
- 空 GPU 图片和读取失败路径产生预期异常。

### Python 测试

更新 `tests/test_python_pipeline_orchestrator.py`：

1. 调用 `read_image()` 得到 `GpuImage`。
2. 将同一对象传给两个 `process_*_frame`。
3. 验证处理阶段不生成输出图片。
4. 调用两个 `save_*_frame_results`。
5. 验证图片文件最终存在。
6. 验证便捷接口仍然可以完成处理和保存。

## 兼容性与风险

- 这是逐帧处理接口的行为变更：`process_*_frame` 不再接收路径，而接收
  `GpuImage`。
- 现有 Python 调用方需要先调用 `read_image()`。
- 结果 ID 的保存模式继续保留，降低调用方迁移成本。
- GPU 图片对象跨 Python/C++ 边界时依赖 pybind11 的对象生命周期管理；对象只
  持有 `cv::cuda::GpuMat`，不暴露裸 GPU 指针。
- 同一输入可被两个流程复用，但两个 remap 仍会各自产生自己的 GPU 输出。
- 保存多个结果时，每个结果在保存时下载一次，保持实现简单并避免提前占用 CPU
  内存。

## 验收标准

- `read_image(path)` 返回 GPU 图片对象。
- `process_virtual_camera_frame` 和 `process_undistort_frame` 只接收 GPU 图片
  并返回 GPU 图片结果。
- 处理阶段没有输入 `GpuMat.upload()` 和输出 `download()`。
- `save_*_frame_results` 调用时才完成结果下载和写盘。
- 同一张输入图片可用于两种处理流程。
- C++ 与 Python 测试在 CUDA 环境下通过。
- 现有 artifacts、输出路径和文件命名保持不变。
