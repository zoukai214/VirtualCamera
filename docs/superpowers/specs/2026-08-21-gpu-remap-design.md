# GPU Remap 专用分支设计

## 背景

当前去畸变和虚拟相机流程已经将配置加载、标定加载、映射表生成、缓存构建、逐帧处理、结果保存拆开。两条图像处理路径的逐帧重映射都直接调用 CPU 版 `cv::remap`：

- 去畸变：`src/undistort_processor.cpp`
- 虚拟相机：`src/virtual_camera_processor.cpp`

当前仓库自带的 `third_party/opencv` 不包含 `cudawarping` 模块，无法直接提供 `cv::cuda::remap`。本机环境存在 NVIDIA RTX 3060、CUDA 工具链，并且 `/opt/opencv-cuda` 中的 OpenCV CUDA 版本可以编译和运行 `cv::cuda::remap`。

本分支是 GPU remap 专用开发分支，不要求保持 CPU remap 兼容运行。

## 目标

将去畸变和虚拟相机的逐帧 remap 改为 GPU 版本，直接使用 `cv::cuda::remap`。数据加载、标定解析、映射表生成、bin/json 输出保持现有 CPU 逻辑。

## 非目标

- 不实现 CPU remap fallback。
- 不重新实现 CUDA kernel。
- 不改变配置文件字段和输出目录结构。
- 不改变映射表生成算法。
- 不调整 CMake 编译优化选项。

## 架构

### OpenCV 依赖

CMake 改为链接 `/opt/opencv-cuda` 提供的 OpenCV CUDA 库。实现代码需要包含：

```cpp
#include <opencv2/core/cuda.hpp>
#include <opencv2/cudawarping.hpp>
```

需要链接的模块至少包括：

- `opencv_core`
- `opencv_imgproc`
- `opencv_imgcodecs`
- `opencv_calib3d`
- `opencv_cudawarping`
- `opencv_cudaarithm`

项目不再依赖当前 vendored `libopencv_world.a` 作为主 OpenCV 链路。

### 缓存结构

现有 cache entry 保留 CPU 版 `cv::Mat` 映射表，用于保存 bin 和测试比对；同时新增 GPU 版映射表成员：

```cpp
cv::cuda::GpuMat gpu_map_x;
cv::cuda::GpuMat gpu_map_y;
```

去畸变 cache entry 和虚拟相机 cache entry 都按这个方式扩展。

构建 cache 时仍先调用现有 CPU 映射表生成函数：

- `GenerateUndistortMaps`
- `GenerateVirtualCameraMaps`

生成完成后立即上传：

```cpp
entry.gpu_map_x.upload(entry.maps.map_x);
entry.gpu_map_y.upload(entry.maps.map_y);
```

虚拟相机的 `src_map_x/src_map_y` 只用于保存映射表文件，不参与逐帧 remap，因此不需要上传 GPU。

### 逐帧处理

去畸变和虚拟相机逐帧处理流程统一为：

1. 输入 `cv::Mat image`
2. 上传到 `cv::cuda::GpuMat gpu_image`
3. 使用 cache entry 中的 `gpu_map_x/gpu_map_y` 调用 `cv::cuda::remap`
4. 下载为 `cv::Mat result.image`
5. 复用现有保存逻辑

示意代码：

```cpp
cv::cuda::GpuMat gpu_input;
gpu_input.upload(image);

cv::cuda::GpuMat gpu_output;
cv::cuda::remap(gpu_input, gpu_output, entry.gpu_map_x, entry.gpu_map_y,
                cv::INTER_LINEAR);

gpu_output.download(result.image);
```

第一版不引入跨帧 GPU input/output buffer 复用。当前处理函数按输入帧返回多个结果，先保持局部变量实现，降低生命周期复杂度。

### GPU 可用性检查

因为本分支是 GPU 专用分支，运行时不做 CPU fallback。pipeline 开始构建 cache 前检查：

```cpp
if (cv::cuda::getCudaEnabledDeviceCount() <= 0) {
  throw std::runtime_error("CUDA remap requires an available CUDA device");
}
```

去畸变和虚拟相机入口都执行检查，错误应尽早暴露，避免处理到中途才失败。

## 数据流

### 去畸变

1. `BuildUndistortCache`
2. 加载标定
3. CPU 生成 `map_x/map_y`
4. 上传 `gpu_map_x/gpu_map_y`
5. `ProcessUndistortFrame` 使用 `cv::cuda::remap`
6. `SaveUndistortFrameResult` 保持不变

### 虚拟相机

1. `BuildVirtualCameraCache`
2. 加载标定
3. CPU 生成 `map_x/map_y/src_map_x/src_map_y`
4. 上传 `gpu_map_x/gpu_map_y`
5. 保存现有 map bin 和 virtual json
6. `ProcessVirtualCameraFrame` 使用 `cv::cuda::remap`
7. `SaveVirtualCameraFrameResult` 保持不变

## 错误处理

- 无 CUDA 设备：入口直接抛出 `std::runtime_error`。
- OpenCV CUDA remap 抛异常：向上传递异常，不做 CPU fallback。
- 图片读取失败、图片保存失败、重复输出路径校验保持现有行为。

## 测试计划

新增或调整手写 C++ 测试：

- GPU identity remap 测试：构造小图和 identity 映射表，调用 `cv::cuda::remap`，验证输出像素一致。
- 去畸变 cache 测试：构建 cache 后验证 `gpu_map_x/gpu_map_y` 非空，尺寸与 CPU map 一致。
- 虚拟相机 cache 测试：构建 cache 后验证 `gpu_map_x/gpu_map_y` 非空，尺寸与 CPU map 一致。
- 去畸变处理测试：`ProcessUndistortFrame` 输出数量、尺寸、任务元数据保持现有预期。
- 虚拟相机处理测试：`ProcessVirtualCameraFrame` 输出数量、尺寸、任务元数据保持现有预期。
- 无 CUDA 设备测试不作为本分支主路径要求；当前分支运行测试需要 CUDA 环境。

验证命令：

```bash
cmake -S . -B build
cmake --build build -j
./build/test_remap_generator
./build/test_undistort_processor
./build/test_virtual_camera_processor
./build/test_pipeline_config
./build/test_pipeline_orchestrator
```

如构建改为 catkin 环境，使用项目现有 catkin 构建命令替代第一步和第二步，但测试可执行文件保持逐个运行。

## 风险

- `/opt/opencv-cuda` 与当前 vendored OpenCV 版本不同，切换链接后可能暴露 ABI 或模块依赖问题。
- GPU remap 每帧仍有上传和下载成本，小图或 IO 受限场景可能收益有限。
- 多线程并发处理多相机时会并发使用同一 GPU，需要后续用 benchmark 验证最优并行参数。

## 验收标准

- 去畸变和虚拟相机逐帧 remap 不再调用 CPU `cv::remap`。
- CUDA 环境可用时，相关单元测试通过。
- 输出图片、json、bin 路径和文件命名与现有行为一致。
- 无 CUDA 设备时，程序明确报错，不静默退回 CPU。
