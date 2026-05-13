# VirtualCamera

RT024 数据处理工具，用于生成去畸变参数、虚拟相机参数、去畸变图片、虚拟相机图片和虚拟相机映射表。

## 功能概览

- `process_undistort`：生成去畸变参数和去畸变图片
- `process_virtual_camera`：生成虚拟相机参数、虚拟相机图片和 `bin` 映射表
- 支持串行执行，也支持任务级和相机级并行执行
- 支持与真值目录做自动结果校验

## 编译

当前分支的标准编译命令：

```bash
bash scripts/build.sh
```

编译完成后，可执行文件位于：

```bash
./build/virtual_camera_tool
```

## 运行前准备

运行 RT024 流程前，需要保证虚拟相机映射库路径已加入 `LD_LIBRARY_PATH`：

```bash
export LD_LIBRARY_PATH=/workspace/gen_vc_bin_lib_test/deps/x86/gen_vc_map_lib/lib:${LD_LIBRARY_PATH:-}
```

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
export LD_LIBRARY_PATH=/workspace/gen_vc_bin_lib_test/deps/x86/gen_vc_map_lib/lib:${LD_LIBRARY_PATH:-}
./build/virtual_camera_tool configs/config_rt024.json
```

默认输出目录：

```bash
build/rt024_output
```

## 并行配置运行

本分支已验证通过的并行配置文件：

```bash
configs/config_rt024_parallel_run.json
```

运行命令：

```bash
export LD_LIBRARY_PATH=/workspace/gen_vc_bin_lib_test/deps/x86/gen_vc_map_lib/lib:${LD_LIBRARY_PATH:-}
./build/virtual_camera_tool configs/config_rt024_parallel_run.json
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

## 输出内容

程序会在 `output_root` 下生成：

- `calib_undistortion`
- `calib_virtual_camera`
- `image_undistortion`
- `image_virtual_camera`
- `vc_gdcbin_dir_path`

## 校验与测试

程序运行结束后会自动做结果校验。当前自动校验覆盖：

- `vc_gdcbin_dir_path` 中的虚拟相机映射表逐字节一致
- `calib_undistortion` 中的参数 JSON 一致
- `calib_virtual_camera` 中的参数 JSON 一致

校验通过时输出：

```text
verification passed
```

当前分支本次回归中，以下命令已验证通过：

```bash
./build/test_parallel_executor
./build/test_pipeline_runner
./build/test_undistort_processor
./build/test_virtual_camera_processor
./build/test_pipeline_config
./build/test_rt024_json_writer
./build/test_rt024_verifier
./build/virtual_camera_tool configs/config_rt024_parallel_run.json
```
