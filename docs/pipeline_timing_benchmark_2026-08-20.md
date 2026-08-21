# Pipeline Timing Benchmark 2026-08-20

## 测试目标

本次测试在单进程、单线程条件下统计 RT024 测试数据的主要流水线耗时：

- config 加载及映射表生成时间
- 去畸变读取图片时间
- 去畸变处理时间
- 保存去畸变图片时间
- 虚拟相机读取图片时间
- 虚拟相机处理时间
- 保存虚拟相机图片时间

## 测试命令

```bash
cmake -S . -B /tmp/virtual_camera_benchmark_build
cmake --build /tmp/virtual_camera_benchmark_build --target benchmark_pipeline_timing -j2
/tmp/virtual_camera_benchmark_build/benchmark_pipeline_timing \
  --dataset_root /workspace/GACRT024_1754812994 \
  --config_path configs/config_rt024.json \
  --output_root /tmp/virtual_camera_timing_output_20260820
```

## 测试配置

| 项目 | 值 |
| --- | --- |
| 测试时间 | 2026-08-20 15:03:40 UTC |
| 工作目录 | `/workspace/VirtualCamera` |
| 数据集 | `/workspace/GACRT024_1754812994` |
| 配置文件 | `configs/config_rt024.json` |
| 输出目录 | `/tmp/virtual_camera_timing_output_20260820` |
| OpenCV 线程数 | 1 |
| 测试进程数 | 1 |
| 输入图片总数 | 1582 |
| 每路相机图片数 | 226 |
| 相机目录 | `back`, `front_narrow`, `front_wide`, `left_back`, `left_front`, `right_back`, `right_front` |
| 输出图片总数 | 4294 |
| 输出目录大小 | 1.4G |

## 测试结果

config 加载及映射表生成总耗时：14696.584 ms。

| 环节 | 输入图片数 | 输出图片数 | 总耗时 ms | 平均每输入图 ms | 平均每输出图 ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| 去畸变读取图片 | 1582 | 0 | 80508.105 | 50.890 | 0.000 |
| 去畸变处理 | 1582 | 1582 | 124692.348 | 78.819 | 78.819 |
| 保存去畸变图片 | 1582 | 1582 | 119794.715 | 75.724 | 75.724 |
| 虚拟相机读取图片 | 1582 | 0 | 80576.052 | 50.933 | 0.000 |
| 虚拟相机处理 | 1582 | 2712 | 26056.837 | 16.471 | 9.608 |
| 保存虚拟相机图片 | 1582 | 2712 | 26104.216 | 16.501 | 9.625 |

## 真实并行测试

本次额外使用真实并行分阶段 benchmark `benchmark_pipeline_timing_parallel` 复测，配置参数为：

- `task_parallelism = 2`
- `undistort_parallelism = 2`
- `virtual_camera_parallelism = 2`

测试命令：

```bash
/workspace/VirtualCamera/build/benchmark_pipeline_timing_parallel \
  --dataset_root /workspace/GACRT024_1754812994 \
  --config_path /workspace/VirtualCamera/configs/config_rt024_parallel_run.json \
  --output_root /tmp/virtual_camera_timing_output_20260820_real_parallel_stages
```

| 项目 | 值 |
| --- | --- |
| 测试时间 | 2026-08-21 05:47:06 UTC |
| 输出目录 | `/tmp/virtual_camera_timing_output_20260820_real_parallel_stages` |
| 输出目录大小 | 1.9G |
| OpenCV 线程数 | 1 |
| 真实总耗时 | 175341.751 ms |
| undistort pipeline elapsed | 175341.552 ms |
| virtual_camera pipeline elapsed | 70384.993 ms |
| config 加载及映射表生成总耗时 | 14718.386 ms |

| 环节 | 输入图片数 | 输出图片数 | 总耗时 ms | 平均每输入图 ms | 平均每输出图 ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| 去畸变读取图片 | 1582 | 0 | 80372.680 | 50.804 | 0.000 |
| 去畸变处理 | 1582 | 1582 | 127819.407 | 80.796 | 80.796 |
| 保存去畸变图片 | 1582 | 1582 | 120123.524 | 75.931 | 75.931 |
| 虚拟相机读取图片 | 1582 | 0 | 80845.769 | 51.104 | 0.000 |
| 虚拟相机处理 | 1582 | 2712 | 26509.971 | 16.757 | 9.775 |
| 保存虚拟相机图片 | 1582 | 2712 | 26297.134 | 16.623 | 9.697 |

真实并行分阶段 benchmark 同时调度去畸变和虚拟相机两个主流程，并在各主流程内部按相机目录并行执行。分阶段耗时为所有工作线程的累计耗时，可能大于对应 pipeline 的 wall-clock。

## GPU Remap 验证与 Benchmark

本次在 GPU remap 分支上额外做了两类验证：

1. `--debug` 模式下对照 golden 输出
2. 按相同模块口径的并行分阶段 benchmark

### Debug 验证

测试命令：

```bash
./build/virtual_camera_tool \
  --dataset_root /workspace/GACRT024_1754812994 \
  --config_path configs/config_rt024_parallel_run.json \
  --output_root build/gpu_remap_debug_output_20260821_0715 \
  --debug \
  --golden_root /workspace/GACRT024_1754812994
```

结果：

- 流水线可以完整跑完
- 最终 golden 比对失败
- byte mismatch 文件：
  - `vc_gdcbin_dir_path/fr99_vc_mapX.bin`
  - `vc_gdcbin_dir_path/fw110_vc_mapY.bin`
  - `vc_gdcbin_dir_path/rl99_vc_mapY.bin`
- 差异是单字节级别偏移，属于 OpenCV CUDA/OpenCV 版本切换后的数值漂移现象

### GPU Benchmark

测试命令：

```bash
./build/benchmark_pipeline_timing_parallel \
  --dataset_root /workspace/GACRT024_1754812994 \
  --config_path /workspace/VirtualCamera/configs/config_rt024_parallel_run.json \
  --output_root build/gpu_remap_benchmark_output_20260821_0715
```

| 项目 | 值 |
| --- | --- |
| 测试时间 | 2026-08-21 07:15 UTC |
| 输出目录 | `build/gpu_remap_benchmark_output_20260821_0715` |
| OpenCV 线程数 | 1 |
| 真实总耗时 | 40352.968 ms |
| undistort pipeline elapsed | 40352.728 ms |
| virtual_camera pipeline elapsed | 26446.702 ms |
| config 加载及映射表生成总耗时 | 14294.938 ms |

| 环节 | 输入图片数 | 输出图片数 | 总耗时 ms | 平均每输入图 ms | 平均每输出图 ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| 去畸变读取图片 | 1582 | 0 | 27478.776 | 17.370 | 0.000 |
| 去畸变处理 | 1582 | 1582 | 8683.141 | 5.489 | 5.489 |
| 保存去畸变图片 | 1582 | 1582 | 41296.450 | 26.104 | 26.104 |
| 虚拟相机读取图片 | 1582 | 0 | 27805.203 | 17.576 | 0.000 |
| 虚拟相机处理 | 1582 | 2712 | 11707.268 | 7.400 | 4.317 |
| 保存虚拟相机图片 | 1582 | 2712 | 10693.174 | 6.759 | 3.943 |

## 环境信息

| 项目 | 值 |
| --- | --- |
| OS | Ubuntu 20.04.6 LTS |
| Kernel | Linux 6.8.0-106-generic x86_64 |
| CPU | Intel(R) Core(TM) i7-8700T CPU @ 2.40GHz |
| CPU 核心 | 6 cores / 12 threads |
| 内存 | 31Gi total, 24Gi available |
| Swap | 2.0Gi |
| `/workspace` 磁盘 | 439G total, 174G available |
| 编译器 | g++ 9.4.0 |
| CMake | 3.16.3 |

## 主要依赖库版本

| 依赖库 | 版本 | 来源 |
| --- | --- | --- |
| OpenCV | 3.4.5 | `third_party/opencv/include/opencv2/core/version.hpp` |
| Eigen | 3.3.9 | `third_party/eigen/include/eigen3/Eigen/src/Core/util/Macros.h` |
| nlohmann_json | 3.7.3 | `third_party/nlohmann_json/include/nlohmann_json/json.hpp` |

## 说明

- 单线程 benchmark 可执行文件为 `benchmark_pipeline_timing`，代码位于 `tests/benchmark_pipeline_timing.cpp`。
- 单线程 benchmark 使用顺序循环执行，不调用 pipeline 并行调度器。
- 真实并行分阶段 benchmark 可执行文件为 `benchmark_pipeline_timing_parallel`，代码位于 `tests/benchmark_pipeline_timing_parallel.cpp`。
- 真实并行分阶段 benchmark 通过 `task_parallelism=2` 同时调度两个主流程，并通过各自的 pipeline parallelism 按相机目录并行。
- benchmark 启动后调用 `cv::setNumThreads(1)`，限制 OpenCV 内部线程数为 1。
- config 加载及映射表生成耗时包含 `LoadPipelineConfig`、`BuildUndistortCache`、`BuildVirtualCameraCache`。
- 图片读取时间使用 `cv::imread` 计时，图片保存时间使用 `cv::imwrite` 计时。
- 本次结果包含实际落盘成本，受当前磁盘状态、文件系统缓存和 JPEG 编解码开销影响。
