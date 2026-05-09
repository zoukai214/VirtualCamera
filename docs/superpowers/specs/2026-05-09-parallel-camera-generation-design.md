# 并行相机生成设计

## 背景

当前 `generate-verify` 在主线程中顺序遍历所有 7v 任务，每个任务独立加载标定、生成映射并写出结果。`generate-4v` 的四相机映射和圆柱映射也按相机顺序执行。resize、去畸变、虚拟相机和 4v 单相机映射之间大多没有数据依赖，适合用受控并行提升生成速度。

## 目标

- `generate-verify` 支持多个 7v 相机任务并行生成。
- `generate-4v` 支持 4v 四相机映射和圆柱映射并行生成。
- 默认自动使用硬件并发数，同时支持 `--jobs N` 手动限制并发。
- 保持输出文件内容与现有真值一致。
- 保留 `--jobs 1` 作为串行退化路径，便于排查问题。

## 命令行接口

`generate-verify` 增加可选参数：

```bash
./build/virtual_camera_tool generate-verify <input_root> <config_path> <output_root> [--jobs N]
```

`generate-4v` 增加可选参数：

```bash
./build/virtual_camera_tool generate-4v <config.yaml> [--jobs N]
```

不传 `--jobs` 或传入 `0` 时，使用 `std::thread::hardware_concurrency()`。如果运行环境无法返回有效并发数，则回退到 1。负数、非数字、缺少参数等非法输入返回命令行错误。

## 7v 并行模型

`BuildThorTasks()` 仍负责生成任务列表。`GenerateVerify()` 将任务列表交给一个轻量并行执行器，最多同时运行 `jobs` 个任务。

每个 `CameraTask` 是一个独立并行单元：

1. 加载该任务的标定。
2. 创建 `MapGenerator`。
3. 按任务类型生成 map。
4. 写出对应 bin。
5. resize 和 virtual 任务按原逻辑写出 virtual json。

所有任务完成后，主线程再执行 `NormalizeToGolden()` 和 `VerifyOutputs()`。这样验证阶段不会与写文件并发，输出目录结构也保持现有行为。

错误处理采用线程内捕获、主线程汇总的方式。任意任务失败时，本轮生成失败并返回非 0；错误信息包含任务类型、bin 名和异常内容。

## 4v 并行模型

`RunFourViewGenerate()` 接收 `jobs` 参数，并传入 4v 生成流程。

`CameraMapsGenerator::generateCameraMapsOptimized()` 增加 `jobs` 参数：

- 第一步坐标映射：front、rear、left、right 四个相机区域并行。每个线程只写自己的 `CameraMap`，避免共享写入。
- front/rear 权重阶段：该阶段可能通过 `WeightCalculator` 写入共享 `weight_2_dim`，先保持串行，避免改变结果。
- left/right 权重阶段：依赖 `weight_2_dim` 完成后执行，可按相机并行写入各自 `CameraMap`。
- 归一化阶段：`total_weight` 汇总保持串行；每个相机的归一化结果可并行写回各自 `CameraMap`。

`GenerateCylinderMaps()` 的四个圆柱相机互相独立，按相机并行生成并写出 `gdc_<name>.bin`。每个线程写不同文件。

## 并行执行器

新增一个小型通用工具函数，不引入复杂线程池类型：

- 输入任务数量、最大并发数和任务函数。
- 使用 `std::atomic<size_t>` 分发任务索引。
- 创建 `min(jobs, task_count)` 个 worker 线程。
- worker 捕获异常并记录首个或全部错误。
- 主线程 join 所有 worker 后，如有错误则抛出聚合异常。

该工具可复用于 7v task、4v 相机区域、圆柱相机和归一化阶段。

## 测试方案

单元测试：

- 覆盖 `--jobs` 参数解析。
- 覆盖默认 jobs、`--jobs 0` 自动回退、`--jobs 1` 串行、非法参数报错。
- 覆盖并行执行器能处理全部任务，并能把 worker 异常传回主线程。

回归验证：

- 7v 输入：`/workspace/L022/cfg/7v`。
- 7v 真值：`/workspace/L022/cfg/7v/calib/gdc`、`gdc_intri`、`virtual`。
- 4v 输入：`/workspace/L022/cfg/4v`。
- 4v 真值：`/workspace/L022/cfg/4v/maps`。
- 分别运行串行和并行：

```bash
./build/virtual_camera_tool generate-verify /workspace/L022/cfg/7v configs/config_thor.json output_verify/7v --jobs 1
./build/virtual_camera_tool generate-verify /workspace/L022/cfg/7v configs/config_thor.json output_verify/7v --jobs 4
./build/virtual_camera_tool generate-4v configs/config_4v.yaml --jobs 1
./build/virtual_camera_tool generate-4v configs/config_4v.yaml --jobs 4
```

脚本 `scripts/run_thor_verify.sh` 和 `scripts/run_4v_verify.sh` 将保留默认调用路径，确保不传 `--jobs` 时也能通过。

## 风险与处理

- OpenCV 内部可能已有并行化。外层 jobs 过大时可能导致 CPU 过载，因此提供 `--jobs N` 限制。
- 多线程日志可能交错。核心验证依赖文件结果，日志只用于定位问题，必要时后续再做日志串行化。
- 4v 权重计算中 `weight_2_dim` 是共享状态，front/rear 权重阶段先保持串行，优先保证结果稳定。
- 输出文件路径必须互不冲突。7v 任务使用既有 bin/json 文件名；如果配置中存在重复输出名，并行会暴露覆盖风险。当前配置中 undistort 任务已按 camera_id 去重，resize 和 virtual 文件名不同。
