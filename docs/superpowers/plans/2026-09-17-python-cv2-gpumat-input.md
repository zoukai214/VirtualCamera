# Python 直传 cv2.cuda_GpuMat 输入实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让 Python 侧可以将 `cv2.cuda_GpuMat` 对象直接传给 `process_virtual_camera_frame()` 与 `process_undistort_frame()`，不再必须通过 `read_image()` 获取 `virtual_camera.GpuImage`。

**Architecture:** 在 `src/python_bindings.cpp` 中定义 `pybind11::detail::type_caster<cv::cuda::GpuMat>`：从 `cv2.cuda_GpuMat` 对象提取设备指针与元信息（`cudaPtr()`、`rows`、`cols`、`type()`、`step`），以外部内存方式构造 `cv::cuda::GpuMat` 后立即 `clone()` 为自有显存，切断对 Python 对象生命周期的依赖。`PyPipelineOrchestrator` 的两个处理函数增加 `cv::cuda::GpuMat` 重载，内部包装成 `vc::GpuImage` 转发给 orchestrator。绑定层用 `py::overload_cast` 同时注册 `vc::GpuImage` 与 `cv::cuda::GpuMat` 两个重载，保留原有 `read_image()` 工作流，核心 C++ 库（cuda_remap、processor、orchestrator）零改动。

**Tech Stack:** C++17、OpenCV CUDA (`/opt/opencv-cuda`)、pybind11 2.13.6、Python 3.8/3.10、带 CUDA 构建的 `cv2`（仅运行直传用例时需要）、现有手写 C++ 测试与 Python workflow 测试。

---

## 文件变更地图

- Create: `docs/superpowers/plans/2026-09-17-python-cv2-gpumat-input.md`，记录本实现计划。
- Modify: `src/python_bindings.cpp`，定义 `type_caster<cv::cuda::GpuMat>`、处理函数重载与绑定注册。
- Modify: `tests/test_python_pipeline_orchestrator.py`，增加 cv2.cuda_GpuMat 直传用例（cv2 无 CUDA 时跳过）。
- Modify: `README.md`，更新 Python 接口说明与直传示例。

## 关键设计决策（实现时不得再改）

1. **转换方向**：只实现 Python → C++ 的 `load()`；`cast()` 返回 `handle()`，不支持 C++ → Python 方向（本模块不需要向 Python 返回 `GpuMat`）。
2. **内存所有权**：`load()` 中用 `cudaPtr()` 构造的 `GpuMat` 是外部内存视图，必须在 `load()` 内 `clone()` 为自有显存。否则 Python 对象被 GC 释放显存后，wrapper 缓存在 `virtual_results_by_id_` / `undistort_results_by_id_` 中的结果会变成悬空指针。
3. **类型校验**：用 `py::isinstance(src, cv2.cuda_GpuMat)` 严格校验，不做 duck-typing。
4. **错误信息**：`cv2` 导入失败或 `cv2` 无 `cuda_GpuMat` 属性时抛 `py::type_error`，消息中说明"cv2 build has no CUDA support"；空 GpuMat 同样抛 `py::type_error`；非 cuda_GpuMat 对象返回 `false` 走 pybind11 标准 TypeError。
5. **参数命名**：新重载的 Python 参数名为 `gpu_mat`，旧 `vc::GpuImage` 重载保持 `image`。
6. **step 处理**：`cv2.cuda_GpuMat.step` 可能为 0（紧凑布局），`cv::cuda::GpuMat(rows, cols, type, ptr, step)` 构造器已按 `cols * elemSize()` 处理 0 值，无需额外代码。
7. **GIL**：caster 的 `load()` 在 pybind11 持有 GIL 时执行，直接调用 `attr()` 安全。
8. **双 OpenCV 二进制**：C++ 扩展链接 `/opt/opencv-cuda`，Python 侧 cv2 是独立构建。本方案只通过 Python 属性读取整型元信息并操作显存，不跨 ABI 调用 cv2 的 C++ 函数；同进程同 device 下共享 CUDA primary context，`clone()` 与后续 `cv::cuda::remap` 安全。

---

### Task 1: 写失败测试，固定 cv2.cuda_GpuMat 直传接口

**Files:**
- Modify: `tests/test_python_pipeline_orchestrator.py`

- [x] **Step 1: 增加直传用例与 cv2 CUDA 探测辅助函数**

在 `tests/test_python_pipeline_orchestrator.py` 顶部增加（`import json` 之前保持现有 import 顺序）：

```python
def try_cv2_cuda():
    """返回带 CUDA 的 cv2 模块；环境不可用时返回 None 使直传用例跳过。"""
    try:
        import cv2
    except ImportError:
        return None
    if not hasattr(cv2, "cuda_GpuMat"):
        return None
    if cv2.cuda.getCudaEnabledDeviceCount() <= 0:
        return None
    return cv2


def expect_raises(exception_type, message):
    def decorator(fn):
        def wrapper(*args, **kwargs):
            try:
                fn(*args, **kwargs)
            except exception_type:
                return
            raise RuntimeError(message)
        return wrapper
    return decorator
```

在 `main()` 中 `expect(sources == ...)` 之前增加直传用例调用（放在 orchestrator 构造之后、现有断言之前）：

```python
cv2 = try_cv2_cuda()
if cv2 is None:
    print("skip cv2.cuda_GpuMat input test: cv2 with CUDA not available")
else:
    run_cv2_gpu_mat_input(root, dataset_root, orchestrator, cv2)
```

新增用例函数（放在 `main()` 之前）：

```python
def run_cv2_gpu_mat_input(root, dataset_root, orchestrator, cv2):
    import numpy as np

    # 生成与 write_tiny_ppm 相同的 16x8 彩色图并上传到 GPU。
    height, width = 8, 16
    rows = np.arange(height).reshape(-1, 1)
    cols = np.arange(width).reshape(1, -1)
    rgb = np.zeros((height, width, 3), dtype=np.uint8)
    rgb[:, :, 0] = cols * 7
    rgb[:, :, 1] = rows * 13
    rgb[:, :, 2] = (rows + cols) * 5
    gpu_mat = cv2.cuda_GpuMat()
    gpu_mat.upload(rgb)

    virtual_result_id = orchestrator.process_virtual_camera_frame(1, gpu_mat)
    undistort_result_id = orchestrator.process_undistort_frame(1, gpu_mat)

    output_virtual = root / "cv2_virtual_output"
    output_undistort = root / "cv2_undistort_output"
    output_virtual.mkdir()
    output_undistort.mkdir()
    orchestrator.save_virtual_camera_frame_results(
        virtual_result_id, str(output_virtual), "synthetic_front_wide.ppm"
    )
    orchestrator.save_undistort_frame_results(
        undistort_result_id, str(output_undistort), "synthetic_front_wide.ppm"
    )
    expect((output_virtual / "front_wide_110" / "fw110_synthetic_front_wide.ppm").exists(),
           "cv2.cuda_GpuMat virtual camera input should produce output image")
    expect((output_undistort / "front_wide" / "synthetic_front_wide.ppm").exists(),
           "cv2.cuda_GpuMat undistort input should produce output image")

    # 非 cuda_GpuMat 对象必须被拒绝。
    @expect_raises(TypeError, "non cv2.cuda_GpuMat input should raise TypeError")
    def reject_wrong_type():
        orchestrator.process_virtual_camera_frame(1, "not-a-gpu-mat")

    reject_wrong_type()
```

输出路径命名以现有保存逻辑为准：虚拟相机保存为 `{save_dir}/{file_prefix}_{input_filename}`，去畸变保存为 `{image_dir}/{input_filename}`；若实际测试运行发现路径不同，以现有 GpuImage 路径测试的断言为准修正。

- [x] **Step 2: 运行 Python 测试确认接口尚未实现**

Run:

```bash
cd /mnt/data/virtual_camera/VirtualCamera
python3 tests/test_python_pipeline_orchestrator.py
```

Expected: 若环境 cv2 无 CUDA 则打印 skip 且其余用例通过；若 cv2 带 CUDA 则失败，报 `incompatible function arguments`（当前绑定只接受 `GpuImage`）。

### Task 2: 实现 type caster 与处理函数重载

**Files:**
- Modify: `src/python_bindings.cpp`

- [x] **Step 1: 定义 `type_caster<cv::cuda::GpuMat>`**

在 `src/python_bindings.cpp` 的 `#include` 之后、`namespace py = pybind11;` 之前增加 `<opencv2/core/cuda.hpp>` include，并在文件顶部（`namespace {` 之前）定义：

```cpp
namespace pybind11 { namespace detail {

template <>
struct type_caster<cv::cuda::GpuMat> {
 public:
  PYBIND11_TYPE_CASTER(cv::cuda::GpuMat, _("cv2.cuda_GpuMat"));

  bool load(handle src, bool) {
    py::object cv2;
    try {
      cv2 = py::module_::import("cv2");
    } catch (const py::error_already_set&) {
      throw py::type_error(
          "expected cv2.cuda_GpuMat, but the cv2 module is not installed");
    }
    py::object gpu_mat_type;
    try {
      gpu_mat_type = cv2.attr("cuda_GpuMat");
    } catch (const py::error_already_set&) {
      throw py::type_error(
          "expected cv2.cuda_GpuMat, but this cv2 build has no CUDA support");
    }
    if (!py::isinstance(src, gpu_mat_type)) {
      return false;
    }
    py::object gpu_mat = py::reinterpret_borrow<py::object>(src);
    if (py::cast<bool>(gpu_mat.attr("empty")())) {
      throw py::type_error("cv2.cuda_GpuMat must not be empty");
    }
    void* ptr = reinterpret_cast<void*>(
        py::cast<uintptr_t>(gpu_mat.attr("cudaPtr")()));
    const int rows = py::cast<int>(gpu_mat.attr("rows"));
    const int cols = py::cast<int>(gpu_mat.attr("cols"));
    const int type = py::cast<int>(gpu_mat.attr("type")());
    const size_t step = py::cast<size_t>(gpu_mat.attr("step"));

    // 借用 Python 对象的显存构造视图后立即拷贝为自有内存，避免其被 GC 后悬空。
    const cv::cuda::GpuMat borrowed(rows, cols, type, ptr, step);
    value = borrowed.clone();
    return !value.empty();
  }

  static handle cast(const cv::cuda::GpuMat&, return_value_policy, handle) {
    return handle();
  }
};

}}  // namespace pybind11::detail
```

- [x] **Step 2: `PyPipelineOrchestrator` 增加 `cv::cuda::GpuMat` 重载**

将现有两个处理函数改为转发到 GpuMat 重载，并新增重载：

```cpp
int ProcessVirtualCameraFrame(int camera_id,
                              const vc::GpuImage& image) const {
  return ProcessVirtualCameraFrame(camera_id, image.image);
}

int ProcessVirtualCameraFrame(int camera_id,
                              const cv::cuda::GpuMat& image) const {
  const int result_id = next_result_id_++;
  virtual_results_by_id_[result_id] =
      orchestrator_.ProcessVirtualCameraFrame(camera_id, vc::GpuImage{image});
  return result_id;
}
```

`ProcessUndistortFrame` 使用完全相同的结构（`undistort_results_by_id_` 与 `ProcessUndistortFrame`）。

- [x] **Step 3: 绑定层注册两个重载**

将现有两个 `.def("process_*_frame", ...)` 替换为：

```cpp
.def("process_virtual_camera_frame",
     static_cast<int (PyPipelineOrchestrator::*)(int, const vc::GpuImage&) const>(
         &PyPipelineOrchestrator::ProcessVirtualCameraFrame),
     py::arg("camera_id"), py::arg("image"))
.def("process_virtual_camera_frame",
     static_cast<int (PyPipelineOrchestrator::*)(int, const cv::cuda::GpuMat&) const>(
         &PyPipelineOrchestrator::ProcessVirtualCameraFrame),
     py::arg("camera_id"), py::arg("gpu_mat"))
```

`process_undistort_frame` 相同。其余绑定（`read_image`、`process_and_save_*`、`save_*`）不变。

注意：容器内 pybind11 版本较旧，`py::overload_cast` 对 const 成员函数指针无默认
参数匹配（报 "candidate expects 2 arguments, 1 provided"），统一用 `static_cast`
显式消解重载，兼容所有 pybind11 版本。

- [x] **Step 4: 构建 Python 模块并运行测试**

Run:

```bash
cd /mnt/data/virtual_camera/VirtualCamera
cmake --build build --target virtual_camera -j2
python3 tests/test_python_pipeline_orchestrator.py
```

Expected: 编译成功；原 `read_image()` + `GpuImage` 工作流全部通过；cv2 带 CUDA 时直传用例通过，否则打印 skip。

- [ ] **Step 5: 提交 Python 直传接口**

```bash
git add src/python_bindings.cpp tests/test_python_pipeline_orchestrator.py
git commit -m "feat: accept cv2.cuda_GpuMat directly in Python frame processing"
```

### Task 3: 文档更新与全量回归

**Files:**
- Modify: `README.md`
- Modify: 由测试或编译检查发现的上述实现文件

- [x] **Step 1: 更新 README Python 接口说明**

将 README 中 `read_image()` 说明段落（约 "`read_image()` 会读取路径并上传为不暴露内部实现的 `GpuImage`……" 处）更新为：

- `read_image()` 返回 `GpuImage` 的原有说明保留。
- 增加：两个 `process_*_frame()` 同时接受 `cv2.cuda_GpuMat` 对象直传（要求 Python 环境安装带 CUDA 构建的 cv2，pip 官方 opencv-python 不含 CUDA 模块）；传入后 C++ 侧立即拷贝为自有显存，Python 对象可安全释放。
- 在分离式处理示例后增加直传示例：

```python
import cv2
import numpy as np

gpu_mat = cv2.cuda_GpuMat()
gpu_mat.upload(np.zeros((2160, 3840, 3), dtype=np.uint8))
result_id = orchestrator.process_virtual_camera_frame(1, gpu_mat)
```

- [x] **Step 2: 全量构建与测试**

Run:

```bash
cd /mnt/data/virtual_camera/VirtualCamera
cmake --build build -j2
./build/test_cuda_remap
./build/test_undistort_processor
./build/test_virtual_camera_processor
./build/test_pipeline_orchestrator
./build/test_remap_generator
./build/test_pipeline_config
python3 tests/test_python_pipeline_orchestrator.py
```

Expected: 全部返回 `0`；Python 测试打印 skip 或通过均可（取决于环境 cv2 是否有 CUDA）。

- [x] **Step 3: 真实 cv2.cuda_GpuMat 直传验证（下游环境）**

容器内无带 CUDA 的 cv2（仅 `/opt/opencv-cuda` C++ 库，无 Python 绑定）。
已用 mock cv2 + ctypes 调 libcudart 分配真实显存的等价验证替代
（`data/output/verify_cv2_gpumat.py`）：接口名与 opencv-python 一致的 mock
对象直传 → 处理 → 保存 → 与 `read_image()` 路径输出逐字节一致，负例
TypeError 正常；下游真实 cv2 环境验证仍待执行。

容器内若无带 CUDA 的 cv2，需在具备 CUDA cv2 的目标环境中（cv2 自编译 CUDA 构建）执行：

```bash
python3 -c "
import cv2, numpy as np, virtual_camera
orchestrator = virtual_camera.PipelineOrchestrator(config, dataset_root, 'all')
gpu_mat = cv2.cuda_GpuMat(); gpu_mat.upload(np.zeros((2160, 3840, 3), dtype=np.uint8))
orchestrator.process_virtual_camera_frame(1, gpu_mat)
print('cv2.cuda_GpuMat direct input OK')
"
```

Expected: 打印 OK，无 TypeError。

- [ ] **Step 4: 检查工作区并提交文档**

Run:

```bash
git status --short
git diff HEAD --check
```

Expected: 仅 README.md 等文档修改；`diff --check` 无输出。提交：

```bash
git add README.md docs/superpowers/plans/2026-09-17-python-cv2-gpumat-input.md
git commit -m "docs: document cv2.cuda_GpuMat direct input"
```

## 风险与前提条件

- **cv2 必须是带 CUDA 的构建**：pip 官方 `opencv-python`/`opencv-contrib-python` 不含 CUDA 模块，直传能力依赖下游环境自备 CUDA 版 cv2；环境不满足时直传用例自动跳过，不影响其余功能。
- **宿主机无法做 CUDA 运行验证**（OpenCV `getCudaEnabledDeviceCount` OS call failed），构建与测试必须在容器内完成。
- **交付包不含 cv2**：下游如需直传，需自行准备 CUDA 版 cv2；交付包本身无需打包 cv2。
