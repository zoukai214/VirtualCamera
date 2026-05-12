# 并行化 RT024 处理链路设计

## 目标

在当前项目中，对 `process_undistort` 与 `process_virtual_camera` 两条处理链路做可配置并行优化，并满足以下约束：

- 输出结果与当前串行版本保持一致
- 处理顺序和日志顺序允许变化
- 默认配置保持保守，不改变当前行为
- 不修改 `CMakeLists.txt` 编译选项
- 新功能补充对应单元测试

## 当前问题

当前实现存在两层串行瓶颈：

1. `src/main.cpp` 中先执行 `RunUndistortPipeline(config)`，再执行 `RunVirtualCameraPipeline(config)`
2. `RunUndistortPipeline` 与 `RunVirtualCameraPipeline` 内部都按 task 逐个相机串行处理

这导致：

- 两条主处理链路不能同时运行
- 同一链路中的多个相机不能并行利用 CPU 和 IO
- 大数据集处理耗时随相机数量线性增长

## 设计目标

本次设计的目标是：

- 支持两条主任务并行执行
- 支持 `undistort` 内部多个相机并行执行
- 支持 `virtual camera` 内部多个相机并行执行
- 通过显式配置控制并行度
- 默认全部串行，确保升级后行为稳定

不在本次范围内的内容：

- 不引入 OpenMP、TBB 等新并行框架
- 不自动按 CPU 核数推导并行度
- 不改动现有输出目录结构和文件命名
- 不引入跨 task 共享缓存

## 配置设计

在总配置 JSON 顶层新增 3 个字段：

- `task_parallelism`
- `undistort_parallelism`
- `virtual_camera_parallelism`

字段语义如下：

- `task_parallelism`：控制 `process_undistort` 与 `process_virtual_camera` 两个主任务之间的并行度
- `undistort_parallelism`：控制 `RunUndistortPipeline` 内多个相机 task 的并行度
- `virtual_camera_parallelism`：控制 `RunVirtualCameraPipeline` 内多个相机 task 的并行度

取值规则：

- `<= 1`：按串行执行
- `> 1`：按并行执行
- 实际并发数取 `min(配置值, 可执行 task 数量)`

默认值：

- `task_parallelism = 1`
- `undistort_parallelism = 1`
- `virtual_camera_parallelism = 1`

这样可以保证默认行为与现有版本完全一致。

### 配置示例

完全保持当前串行行为：

```json
{
  "task_parallelism": 1,
  "undistort_parallelism": 1,
  "virtual_camera_parallelism": 1
}
```

只开启主任务并行：

```json
{
  "task_parallelism": 2,
  "undistort_parallelism": 1,
  "virtual_camera_parallelism": 1
}
```

主任务并行，两个子链路内部各开 2 路：

```json
{
  "task_parallelism": 2,
  "undistort_parallelism": 2,
  "virtual_camera_parallelism": 2
}
```

## 并行执行模型

### 主流程

`src/main.cpp` 中保留当前配置解析与最终校验逻辑，但调整执行模型：

1. 读取 `PipelineConfig`
2. 根据 `process_undistort` 与 `process_virtual_camera` 决定需要执行哪些主任务
3. 根据 `task_parallelism` 决定两个主任务串行还是并行执行
4. 等待全部主任务完成
5. 统一执行 `VerifyRt024Outputs`

执行策略：

- `task_parallelism <= 1` 时，保持当前顺序执行
- `task_parallelism > 1` 且两个主任务都开启时，并行执行两个主任务
- 如果只启用一个主任务，则直接执行该任务，不额外制造并发开销

### Undistort 链路

`RunUndistortPipeline` 中，保留单个 task 的处理步骤不变：

1. 读取标定
2. 生成去畸变 map
3. 写去畸变 JSON
4. 遍历输入图像并输出 remap 结果

变更点仅在 task 调度层：

- 现有 `for (const auto& task : config.undistort_tasks)` 改为“按配置并发执行 task”
- 每个 task 独立处理自己的标定、map、输出目录和图像

### Virtual Camera 链路

`RunVirtualCameraPipeline` 中，保留单个 task 的处理步骤不变：

1. 读取源标定
2. 生成虚拟相机 map
3. 写 map 文件
4. 生成虚拟相机 JSON
5. 遍历输入图像并输出 remap 结果

变更点同样只在 task 调度层：

- 现有 `for (const auto& task : config.virtual_tasks)` 改为“按配置并发执行 task”
- 每个 task 独立处理自己的输入、map、JSON 和输出目录

## 并发工具设计

新增一个轻量级并发执行封装，使用 C++17 标准库实现，不引入第三方依赖。

该组件职责仅包括：

- 按给定最大并行度执行一组 job
- 在串行和并行两种模式下提供统一接口
- 收集 job 中抛出的异常
- 在所有已启动 job 收尾后，将失败统一反馈给调用方

建议接口形式：

- 输入：并行度、job 列表
- 输出：无返回值，失败时抛异常

设计原则：

- 并发工具不关心具体业务，只负责调度
- 业务处理逻辑继续保留在现有 processor 中
- 当并行度为 1 时，走同一套接口但按串行执行，避免双路径维护

## 结果一致性设计

本次并行优化必须保证输出结果与当前串行版本一致。

保证方式如下：

1. 不修改单个 task 内部的处理步骤
2. 不修改现有输出路径规则
3. 不修改现有文件命名规则
4. 每个 task 使用独立局部变量，不共享中间状态
5. 仅在“task 调度层”引入并行

当前代码具备天然 task 隔离特征：

- 去畸变 task 写各自的 JSON 与相机图像目录
- 虚拟相机 task 写各自的 map、JSON 与输出图像目录
- `EnsureDirectory` 基于 `std::filesystem::create_directories`，允许多线程重复创建同一目录

因此只要不让多个 task 写入同一输出文件，就可以保证结果稳定。

## 异常处理设计

并行后，异常需要统一收口，避免子线程静默失败。

策略如下：

- 任一 job 失败时，记录第一条异常
- 其他已启动 job 正常收尾
- 所有 job 完成或收尾后，由调用方统一抛出异常
- 主流程在任一处理链路失败时直接返回失败，不执行成功路径
- 最终校验 `VerifyRt024Outputs` 只在两个主任务都完成后执行

这样可以保证：

- 错误不会被吞掉
- 返回语义与当前版本一致
- 不出现主线程提前退出而后台仍在写文件的情况

## 测试策略

本次功能必须补充单元测试，覆盖以下内容。

### 1. 配置解析测试

验证新增字段的解析行为：

- 缺省时默认为 `1`
- 显式配置时能正确读取
- `1` 表示串行
- `>1` 表示允许并行

建议补充到现有 `tests/test_pipeline_config.cpp`

### 2. 并发执行器测试

验证通用并发工具：

- 并行度为 `1` 时可按串行执行全部 job
- 并行度大于 `1` 时可执行全部 job
- 某个 job 抛异常时，最终能正确失败
- 多个 job 同时成功时，不遗漏任务

建议新增独立测试文件，例如：

- `tests/test_parallel_executor.cpp`

### 3. 流程级一致性测试

如果当前测试框架与样例数据足够支撑，补一类轻量流程测试：

- 串行配置运行后记录输出文件集合
- 并行配置运行后记录输出文件集合
- 验证两者文件数量、路径集合与关键内容一致

如果现有测试不适合跑完整图像流程，则至少保证：

- 配置测试完整
- 并发工具测试完整
- 现有验证脚本可以手工用于串并行结果对比

## 代码改动范围

预计涉及以下文件：

- `include/virtual_camera/pipeline_config.h`
- `src/pipeline_config.cpp`
- `src/main.cpp`
- `src/undistort_processor.cpp`
- `src/virtual_camera_processor.cpp`
- 新增并发工具头文件与实现文件
- `tests/test_pipeline_config.cpp`
- 新增并发工具测试文件

不计划修改：

- `CMakeLists.txt` 中编译选项
- 现有 JSON 输出格式
- 现有 map 文件格式

## 调参建议

推荐从保守配置开始：

```json
{
  "task_parallelism": 1,
  "undistort_parallelism": 1,
  "virtual_camera_parallelism": 1
}
```

建议调优顺序：

1. 先将 `task_parallelism` 调到 `2`
2. 观察 CPU、磁盘 IO 和总耗时
3. 再逐步把 `undistort_parallelism` 与 `virtual_camera_parallelism` 提升到 `2`
4. 如资源仍充足，再尝试 `4`

不建议一开始把相机并行度设太大，因为当前链路包含大量：

- `cv::imread`
- `cv::remap`
- `cv::imwrite`

在磁盘 IO 较重时，过高并行度可能导致收益下降甚至变慢。

## 实施结论

推荐采用“标准库并发执行封装 + 三个显式并行度配置”的方案：

- 默认安全
- 配置简单
- 结果一致性可控
- 改动集中
- 易于测试与回归

后续实施应先完成配置解析与并发执行封装，再修改两个 processor 和主流程，最后补全测试并进行串并行结果对比验证。
