# VirtualCamera

7v RT024 数据处理工具，用于生成去畸变参数、虚拟相机参数、去畸变图片、虚拟相机图片和虚拟相机映射表。

## 功能概览

- `process_undistort`：生成去畸变参数和去畸变图片
- `process_virtual_camera`：生成虚拟相机参数、虚拟相机图片和 float `bin` 映射表
- 支持串行执行，也支持任务级和相机级并行执行
- 支持在调试模式下与真值目录做结果校验
- 当前仓库只保留 7v 流程，不再包含 4v 构建和运行入口

## GPU 与 Python 依赖

当前 `data-pipline-gpu-python` 分支是 GPU 专用版本：

- OpenCV 使用 `/opt/opencv-cuda`
- 逐帧去畸变和虚拟相机重映射使用 `cv::cuda::remap`
- 无 CUDA 设备时会直接报错，不会回退到 CPU
- Python 接口通过 CPython 扩展模块暴露 C++ `vc::PipelineOrchestrator`

系统依赖记录在：

```bash
docs/python_dependencies.md
```

pip 依赖记录在：

```bash
requirements-python.txt
```

首次配置环境：

```bash
apt-get update
apt-get install -y python3-pip
python3 -m pip install -r requirements-python.txt
```

## 编译

当前分支的标准编译命令：

```bash
bash scripts/build.sh
```

编译完成后，可执行文件位于：

```bash
./build/virtual_camera_tool
```

Python 扩展模块位于：

```bash
build/python/virtual_camera.cpython-38-x86_64-linux-gnu.so
```

在本仓库内运行 Python 示例或测试前，需要让 Python 找到该模块：

```bash
export PYTHONPATH=/workspace/VirtualCamera/build/python:${PYTHONPATH:-}
```

## 打包

生成下游可直接运行的独立目录：

```bash
cmake -S . -B build
cmake --build build -j"$(nproc)"
rm -rf build/rectify_virtual_camera
cmake --install build --prefix /workspace/VirtualCamera/build/rectify_virtual_camera
```

打包结果目录：

```bash
build/rectify_virtual_camera
```

其中默认配置为并行版本：

```bash
configs/config_rt024_parallel_run.json
```

下游进入包目录后可直接运行：

```bash
cd build/rectify_virtual_camera
bash image_virtual.bash \
  --dataset_root /workspace/GACRT024_1754812994 \
  --config_path ./config.json
```

## 运行前准备

当前分支使用 `/opt/opencv-cuda` 下的 OpenCV CUDA 版本构建，`Eigen` 和
`nlohmann_json` 仍来自仓库内 `third_party`。
运行机器需要可用 NVIDIA GPU、CUDA 运行时和 `/opt/opencv-cuda/lib` 中的 OpenCV CUDA 动态库。

测试数据目录说明：

- 数据根目录：`/workspace/GACRT024_1754812994`
- 输入标定目录：`calib_extract/`
- 输入图片目录：`image_raw/`
- 真值输出目录：
  - `calib_undistortion/`
  - `calib_virtual_camera/`
  - `image_undistortion/`
  - `image_virtual_camera/`
  - `vc_gdcbin_dir_path/`

## 默认配置运行

默认串行配置文件：

```bash
configs/config_rt024.json
```

运行命令：

```bash
./build/virtual_camera_tool \
  --dataset_root /workspace/GACRT024_1754812994 \
  --config_path configs/config_rt024.json
```

默认输出目录：

```bash
/workspace/GACRT024_1754812994
```

## 并行配置运行

本分支已验证通过的并行配置文件：

```bash
configs/config_rt024_parallel_run.json
```

运行命令：

```bash
./build/virtual_camera_tool \
  --dataset_root /workspace/GACRT024_1754812994 \
  --config_path configs/config_rt024_parallel_run.json \
  --output_root build/rt024_output_parallel_run
```

本次验证使用的并行输出目录：

```bash
build/rt024_output_parallel_run
```

对应并行配置为：

```json
{
  "task_parallelism": 2,
  "undistort_parallelism": 2,
  "virtual_camera_parallelism": 2
}
```

## 并行参数说明

新增三个并行参数，未配置时默认按 `1` 处理：

- `task_parallelism`
  - 控制 `process_undistort` 与 `process_virtual_camera` 两个大任务之间的并行度
  - `1` 表示串行执行
  - `2` 表示两个大任务并行执行
- `undistort_parallelism`
  - 控制 `process_undistort` 内多个相机任务的并行度
  - `1` 表示串行
  - `>1` 表示按配置值并行
- `virtual_camera_parallelism`
  - 控制 `process_virtual_camera` 内多个相机任务的并行度
  - `1` 表示串行
  - `>1` 表示按配置值并行

推荐配置方式：

- 保持原行为：

```json
{
  "task_parallelism": 1,
  "undistort_parallelism": 1,
  "virtual_camera_parallelism": 1
}
```

- 只开启大任务并行：

```json
{
  "task_parallelism": 2,
  "undistort_parallelism": 1,
  "virtual_camera_parallelism": 1
}
```

- 开启当前验证通过的并行配置：

```json
{
  "task_parallelism": 2,
  "undistort_parallelism": 2,
  "virtual_camera_parallelism": 2
}
```

调参建议：

- 先从 `2,1,1` 开始，优先观察整体吞吐提升
- 再逐步升到 `2,2,2`
- 不建议第一次直接设置过大，避免磁盘 IO 成为新瓶颈

## 运行日志

`showinfo` 控制 RT024 运行日志：

- `showinfo = 1`：打印流水线和任务级日志
- `showinfo = 0`：只在结束或报错时打印

当前并行配置 `configs/config_rt024_parallel_run.json` 默认启用 `showinfo = 1`。

## 输出内容

程序会在 `output_root` 下生成：

- `calib_undistortion`
- `calib_virtual_camera`
- `image_undistortion`
- `image_virtual_camera`
- `vc_gdcbin_dir_path`

## Python 接口

Python 只暴露一个类：

```python
from virtual_camera import PipelineOrchestrator
```

构造函数：

```python
orchestrator = PipelineOrchestrator(
    config_path="configs/config_rt024_parallel_run.json",
    dataset_root="/workspace/GACRT024_1754812994",
    selection="all",
)
```

`selection` 可选值：

- `"all"`：构建去畸变和虚拟相机 cache
- `"undistort"`：只构建去畸变 cache
- `"virtual_camera"`：只构建虚拟相机 cache

查询输入相机：

```python
virtual_sources = orchestrator.virtual_source_inputs()
undistort_sources = orchestrator.undistort_source_inputs()
```

返回值是字典列表：

```python
[{"camera_id": 1, "image_dir": "front_wide/"}]
```

保存参数和映射表：

```python
orchestrator.save_virtual_camera_artifacts(output_root)
orchestrator.save_undistort_artifacts(output_root)
```

分离式处理和保存虚拟相机图片：

```python
image = orchestrator.read_image(
    "/workspace/GACRT024_1754812994/image_raw/front_wide/source.jpg",
)

result_id = orchestrator.process_virtual_camera_frame(
    camera_id=1,
    image=image,
)

orchestrator.save_virtual_camera_frame_results(
    result_id=result_id,
    output_root="build/python_output",
    input_filename="source.jpg",
)
```

分离式处理和保存去畸变图片：

```python
image = orchestrator.read_image(
    "/workspace/GACRT024_1754812994/image_raw/front_wide/source.jpg",
)

result_id = orchestrator.process_undistort_frame(
    camera_id=1,
    image=image,
)

orchestrator.save_undistort_frame_results(
    result_id=result_id,
    output_root="build/python_output",
    input_filename="source.jpg",
)
```

便捷接口仍然保留，会在一次调用内完成处理和保存：

```python
orchestrator.process_and_save_virtual_camera_frame(
    1,
    "/workspace/GACRT024_1754812994/image_raw/front_wide/source.jpg",
    "build/python_output",
)

orchestrator.process_and_save_undistort_frame(
    1,
    "/workspace/GACRT024_1754812994/image_raw/front_wide/source.jpg",
    "build/python_output",
)
```

`read_image()` 会读取路径并上传为不暴露内部实现的 `GpuImage`。两个
`process_*_frame()` 只接收该 GPU 图片；结果也保留在 GPU，直到
`save_*_frame_results()` 写文件时才下载到 CPU。

## Python 全流程示例

下面示例会保存虚拟相机和去畸变 artifacts，并处理每个输入目录中的图片：

```python
from pathlib import Path

from virtual_camera import PipelineOrchestrator


dataset_root = Path("/workspace/GACRT024_1754812994")
config_path = Path("configs/config_rt024_parallel_run.json")
output_root = Path("build/python_rt024_output")

orchestrator = PipelineOrchestrator(
    str(config_path),
    str(dataset_root),
    "all",
)

orchestrator.save_virtual_camera_artifacts(str(output_root))
orchestrator.save_undistort_artifacts(str(output_root))

for source in orchestrator.virtual_source_inputs():
    image_dir = dataset_root / "image_raw" / source["image_dir"]
    for image_path in sorted(image_dir.iterdir()):
        if not image_path.is_file():
            continue
        image = orchestrator.read_image(str(image_path))
        result_id = orchestrator.process_virtual_camera_frame(
            source["camera_id"],
            image,
        )
        orchestrator.save_virtual_camera_frame_results(
            result_id,
            str(output_root),
            image_path.name,
        )

for source in orchestrator.undistort_source_inputs():
    image_dir = dataset_root / "image_raw" / source["image_dir"]
    for image_path in sorted(image_dir.iterdir()):
        if not image_path.is_file():
            continue
        image = orchestrator.read_image(str(image_path))
        result_id = orchestrator.process_undistort_frame(
            source["camera_id"],
            image,
        )
        orchestrator.save_undistort_frame_results(
            result_id,
            str(output_root),
            image_path.name,
        )
```

## 迁移到其它 Python 项目

推荐把本仓库作为依赖源码一起构建，而不是只复制单个 `.so` 文件：

1. 在目标机器准备 CUDA、NVIDIA 驱动和 `/opt/opencv-cuda`。
2. 安装系统依赖和 pip 依赖：

```bash
apt-get update
apt-get install -y python3-pip
python3 -m pip install -r /path/to/VirtualCamera/requirements-python.txt
```

3. 构建模块：

```bash
cd /path/to/VirtualCamera
cmake -S . -B build
cmake --build build --target virtual_camera -j"$(nproc)"
```

4. 在目标 Python 项目中设置模块路径：

```bash
export PYTHONPATH=/path/to/VirtualCamera/build/python:${PYTHONPATH:-}
```

5. 在 Python 项目中导入：

```python
from virtual_camera import PipelineOrchestrator
```

如果必须复制文件，需要同时保证以下动态库在目标机器可被加载：

- `build/python/virtual_camera*.so`
- `/opt/opencv-cuda/lib/libopencv_*.so*`
- CUDA/NPP 运行时动态库，例如 `/usr/local/cuda/lib64/libnpp*.so*`

可用下面命令检查目标环境缺失的动态库：

```bash
ldd /path/to/virtual_camera.cpython-38-x86_64-linux-gnu.so
```

## 校验与测试

默认运行不会自动做真值校验，只会生成输出。当前调试校验覆盖：

- `calib_undistortion` 中的参数 JSON 一致
- `calib_virtual_camera` 中的参数 JSON 一致
- `vc_gdcbin_dir_path` 中的虚拟相机映射表逐字节一致

图片输出仍会生成，但当前不做真值比对。

调试模式下可显式开启校验：

```bash
./build/virtual_camera_tool \
  --dataset_root /workspace/GACRT024_1754812994 \
  --config_path configs/config_rt024_parallel_run.json \
  --output_root build/rt024_output_parallel_run \
  --debug \
  --golden_root /workspace/GACRT024_1754812994
```

校验通过时输出：

```text
verification passed
```

当前分支本次回归中，以下命令已验证通过：

```bash
./build/test_runtime_args
./build/test_rt024_runtime
./build/test_parallel_executor
./build/test_jobs
./build/test_pipeline_config
./build/test_rt024_verifier
./build/virtual_camera_tool \
  --dataset_root /workspace/GACRT024_1754812994 \
  --config_path configs/config_rt024_parallel_run.json \
  --output_root build/rt024_output_parallel_run
./build/virtual_camera_tool \
  --dataset_root /workspace/GACRT024_1754812994 \
  --config_path configs/config_rt024_parallel_run.json \
  --output_root build/rt024_output_parallel_run \
  --debug \
  --golden_root /workspace/GACRT024_1754812994
```

Python 接口测试命令：

```bash
cmake --build build --target virtual_camera -j"$(nproc)"
python3 tests/test_python_pipeline_orchestrator.py
```

GPU/Python 分支最近验证过的核心命令：

```bash
cmake --build build -j2
python3 tests/test_python_pipeline_orchestrator.py
./build/test_cuda_remap
./build/test_undistort_processor
./build/test_virtual_camera_processor
./build/test_pipeline_orchestrator
```
