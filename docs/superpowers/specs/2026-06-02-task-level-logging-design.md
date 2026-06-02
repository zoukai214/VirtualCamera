# RT024 任务级运行日志设计

## 背景

当前 RT024 流程在执行 `image_virtual.bash` 或 `virtual_camera_tool` 时，中间处理阶段基本无输出。

现状只有两类终端信息：

- 参数错误或运行失败时输出错误信息
- 流程结束时输出最终结果，例如 `verification passed`

这种行为在串行和并行场景下都存在。对长时间运行的数据处理任务来说，调用者无法从终端判断：

- 当前是否已经进入主流程
- 当前在执行 `undistort` 还是 `virtual_camera`
- 当前有哪些任务已经开始或完成
- 并行运行时整体是否仍在推进

用户要求增加“任务级”日志，用于观察运行进度，但不希望细到每张图片都刷屏。

## 目标

1. 为 RT024 主流程增加任务级运行日志。
2. 日志能够覆盖整体入口、两条主流水线和单任务开始/结束。
3. 并行执行时，单条日志必须能独立表达任务身份。
4. 保持最终结果输出不变，仍然打印 `verification passed` 或现有错误信息。
5. 不引入新的命令行参数，不改变当前脚本调用方式。
6. 通过现有配置字段控制日志开关，使当前并行配置开箱即用。

## 非目标

1. 不打印每张图片的处理日志。
2. 不新增 `--verbose`、`--quiet` 等 CLI 参数。
3. 不修改现有去畸变、虚拟相机、校验算法和输出目录结构。
4. 不把日志写入单独文件，本次只考虑终端输出。
5. 不改变失败时的退出码和异常传播方式。

## 方案比较

### 方案 A：始终输出任务级日志

做法：

- 无条件打印流水线和任务级日志。

优点：

- 实现最直接。
- 不需要额外开关。

缺点：

- 所有运行场景都会变吵。
- 不适合后续批处理或脚本化消费。

### 方案 B：复用 `showinfo` 控制任务级日志

做法：

- 继续使用配置文件里的 `showinfo` 字段。
- `showinfo == 1` 时打印任务级日志。
- `showinfo == 0` 时保持当前静默行为。

优点：

- 不新增命令行参数和脚本接口。
- 与现有配置结构兼容。
- 当前并行配置已经设置 `showinfo=1`，改完即可生效。

缺点：

- `showinfo` 原本语义较宽泛，需要在文档中明确它现在承担运行日志开关职责。

### 方案 C：新增 CLI 日志开关

做法：

- 新增 `--verbose` 或等价参数。
- 脚本解析并转发给 `virtual_camera_tool`。

优点：

- 控制最显式。
- 运行时可临时切换。

缺点：

- 需要改 CLI 解析、脚本透传、帮助信息和测试。
- 对当前需求来说接口增量偏大。

## 结论

采用方案 B。

任务级日志由配置字段 `showinfo` 控制：

- `showinfo == 1`：输出运行日志
- `showinfo == 0`：保持当前行为，只在结束或报错时打印

这样可以最小化接口改动，同时满足当前并行配置直接可见进度的诉求。

## 日志范围

### 整体入口日志

在主程序进入顶层流水线前打印一行入口日志，包含：

- `dataset_root`
- `config_path`
- `output_root`
- `debug`

示例：

```text
[INFO] pipeline start: dataset=/workspace/... config=./configs/config_rt024_parallel_run.json output=/workspace/output_dataline debug=true
```

### 流水线级日志

两条顶层流水线分别打印开始和结束日志：

- `undistort start`
- `undistort done`
- `virtual_camera start`
- `virtual_camera done`

开始日志包含：

- 任务数
- 并行度

结束日志包含：

- `elapsed_ms`

示例：

```text
[INFO] undistort start: tasks=7 parallelism=7
[INFO] undistort done: elapsed_ms=23107
[INFO] virtual_camera start: tasks=10 parallelism=10
[INFO] virtual_camera done: elapsed_ms=28764
```

### 任务级日志

单个任务只打印两次：

- `task start`
- `task done`

#### undistort 任务

开始日志包含：

- `image_dir`
- `calib`

结束日志包含：

- `image_dir`
- `elapsed_ms`

示例：

```text
[INFO] undistort task start: image_dir=front_wide/ calib=calib_camera_front_wide_to_car.json
[INFO] undistort task done: image_dir=front_wide/ elapsed_ms=8421
```

#### virtual task

开始日志包含：

- `prefix`
- `save_dir`
- `calib`

结束日志包含：

- `prefix`
- `save_dir`
- `elapsed_ms`

示例：

```text
[INFO] virtual task start: prefix=fw110 save_dir=front_wide_110/ calib=calib_cam_front_wide_fov110.json
[INFO] virtual task done: prefix=fw110 save_dir=front_wide_110/ elapsed_ms=12654
```

## 并行输出行为

并行场景下，不保证日志严格按任务分组输出。

例如两个任务并发执行时，终端可能看到：

```text
[INFO] virtual task start: prefix=fw110 ...
[INFO] virtual task start: prefix=rb99 ...
[INFO] virtual task done: prefix=rb99 ...
[INFO] virtual task done: prefix=fw110 ...
```

这是预期行为，不视为异常。

为避免并发下日志难以理解，设计要求：

1. 每条任务日志必须自带任务标识。
2. 单个任务固定只打印开始和结束两行。
3. 不打印每张图片的明细，避免高并发场景刷屏。

## 输出通道

本次新增日志统一输出到 `stdout`。

原因如下：

- 这些日志属于正常进度信息，不是错误。
- 与最终成功结果共用 `stdout`，便于调用方直接观察处理进度。
- 失败信息和异常仍沿用当前 `stderr` 输出。

## 代码落点

实现集中在三层，避免把日志散落到算法细节中：

### `src/main.cpp`

职责：

- 根据 `showinfo` 打印整体入口日志
- 保留当前最终结果输出

### `src/undistort_processor.cpp`

职责：

- 打印 `undistort` 流水线开始/结束日志
- 打印单个 `undistort task` 开始/结束日志

### `src/virtual_camera_processor.cpp`

职责：

- 打印 `virtual_camera` 流水线开始/结束日志
- 打印单个 `virtual task` 开始/结束日志

### 辅助实现

增加一个轻量日志辅助函数或小型工具，完成以下职责：

- 判断 `showinfo` 是否开启
- 统一拼接 `[INFO]` 单行输出
- 为耗时统计提供小范围复用能力

本次不引入完整日志框架，只做最小封装。

## 耗时统计

新增日志中的耗时统一使用 `elapsed_ms` 表达。

设计选择：

- 使用相对耗时，不输出绝对时间戳
- 先满足“看得到进度”和“知道哪个任务慢”的需求
- 绝对时间戳不属于本次必须能力，避免增加输出噪声

## 对现有行为的影响

### `showinfo == 1`

行为变化：

- 中间过程会有任务级日志
- 最终结果输出保留

### `showinfo == 0`

行为保持不变：

- 不输出中间过程
- 只在结束或报错时打印

### 脚本行为

`image_virtual.bash` 只负责参数解析和转发，不负责新增日志格式。

脚本行为保持简单，所有进度日志由 `virtual_camera_tool` 输出。

## 测试方案

### 单元测试

新增日志相关测试，至少覆盖：

1. `showinfo == 1` 时，能捕获到：
   - 整体入口日志
   - 流水线开始/结束日志
   - 单任务开始/结束日志
2. `showinfo == 0` 时，不应输出新增中间日志。

测试重点是“是否打印”和“打印哪些关键信息”，不强依赖实际耗时数值。

### 回归测试

保留并回归以下能力：

1. 现有脚本二进制定位测试继续通过。
2. 编译和打包流程不受影响。
3. 用户当前使用的脚本命令仍能运行成功。

### 实机验证

使用当前并行配置：

```bash
bash image_virtual.bash \
  --dataset_root /workspace/GACRT024_1754812994 \
  --config_path ./configs/config_rt024_parallel_run.json \
  --debug \
  --golden_root /workspace/GACRT024_1754812994 \
  --output_root /workspace/output_dataline
```

验证点：

- 终端出现任务级日志
- 产物目录正常生成
- 结尾仍为 `verification passed`

## 风险与处理

### 风险 1：并行日志顺序不可预测

处理：

- 每条任务日志携带任务标识
- 测试不依赖并发顺序

### 风险 2：日志实现散落到业务代码

处理：

- 只在主入口和两条流水线边界打印
- 公共格式走统一辅助函数

### 风险 3：日志过多影响可读性

处理：

- 本次严格限制为“流水线级 + 任务级”
- 不进入图片级别

## 实施边界

本次实现只覆盖 RT024 当前主流程涉及的：

- `main`
- `undistort pipeline`
- `virtual_camera pipeline`

不扩展到历史 4v 逻辑，也不扩展到额外打包脚本输出。
