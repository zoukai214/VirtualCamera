# RT024 运行时根路径命令行化设计

## 背景

当前 RT024 主流程通过单个 `config.json` 启动，`dataset_root`、`golden_root`、`output_root` 都写在配置文件里，脚本 `image_virtual.bash` 也固定调用：

```bash
./virtual_camera_tool ./config.json
```

这种方式有几个问题：

- 数据根目录和输出目录属于运行时输入，不适合固化在配置文件中。
- debug 真值对比和正常生成共用同一套配置，使用时容易混淆。
- 脚本缺少显式参数校验，调用者无法直接看出哪些输入是必填。
- 程序入口只能接收配置路径，不利于后续脚本化和自动化调用。

用户要求参考 `/workspace/rectify_virtual_camera` 的执行方式，把运行时根路径从配置文件中移除，并改为显式命令行参数控制。

## 目标

1. `dataset_root`、`output_root`、`golden_root` 从 RT024 配置文件中移除。
2. 程序和脚本统一支持显式参数：

```bash
image_virtual.bash \
  --dataset_root <path> \
  --config_path <path> \
  [--output_root <path>] \
  [--debug --golden_root <path>]
```

```bash
virtual_camera_tool \
  --dataset_root <path> \
  --config_path <path> \
  [--output_root <path>] \
  [--debug --golden_root <path>]
```

3. `--dataset_root` 和 `--config_path` 为必填项。
4. `--output_root` 为选填项，不传时默认等于 `dataset_root`。
5. `--debug` 为显式布尔标志位，默认关闭。
6. 只有在 `--debug` 打开时才执行真值对比；默认不执行真值对比。
7. 开启 `--debug` 时必须输入 `--golden_root`。
8. 保留现有并行任务、任务配置解析和输出目录相对路径规则。

## 非目标

1. 不修改 `CMakeLists.txt` 编译选项。
2. 不改动 RT024 的任务字段语义，例如 `virtual_camera_configs`、`undistort_configs`、并行度字段等。
3. 不改变现有去畸变、虚拟相机和验证算法实现。
4. 不引入新的配置格式，仍然使用 JSON。

## 命令行接口

程序入口由“只接受一个配置文件路径”改为接受显式参数。

推荐用法：

```bash
./build/virtual_camera_tool \
  --dataset_root /workspace/GACRT024_1754812994 \
  --config_path configs/config_rt024_parallel_run.json
```

指定输出目录：

```bash
./build/virtual_camera_tool \
  --dataset_root /workspace/GACRT024_1754812994 \
  --config_path configs/config_rt024_parallel_run.json \
  --output_root build/rt024_output_parallel_run
```

开启 debug 真值对比：

```bash
./build/virtual_camera_tool \
  --dataset_root /workspace/GACRT024_1754812994 \
  --config_path configs/config_rt024_parallel_run.json \
  --output_root build/rt024_output_parallel_run \
  --debug \
  --golden_root /workspace/GACRT024_1754812994
```

参数规则：

- `--dataset_root <path>`：必填。
- `--config_path <path>`：必填。
- `--output_root <path>`：选填，缺省时使用 `dataset_root`。
- `--debug`：可选布尔标志位，不带值。
- `--golden_root <path>`：只允许与 `--debug` 一起使用。

非法组合统一报错：

- 缺少 `--dataset_root`。
- 缺少 `--config_path`。
- 传入 `--debug` 但缺少 `--golden_root`。
- 传入 `--golden_root` 但未开启 `--debug`。
- 未知参数或参数缺值。

## 配置文件边界

RT024 配置文件继续负责描述“如何处理”，不再承担“对哪一份数据处理、输出到哪里、是否做真值对比”。

配置文件保留：

- 相对目录字段：`conf_dir_path`、`image_dir_path`、`vc_image_dir_path`、`undistort_image_dir_path`、`vc_conf_dir_path`、`undistort_conf_dir_path`、`vc_gdcbin_dir_path`
- 流程开关：`process_virtual_camera`、`process_undistort`
- 并行度：`task_parallelism`、`undistort_parallelism`、`virtual_camera_parallelism`
- 任务数组：`virtual_camera_configs`、`undistort_configs`
- 其他算法配置：`undistort_image`、`distort_model`、`save_virtual_json`、`save_undistort_json`、`conf_type` 等

配置文件移除：

- `dataset_root`
- `output_root`
- `golden_root`

这样配置文件可以在不同数据集之间复用，运行时根路径全部由 CLI 决定。

## 程序内部数据流

程序内部拆成两个阶段：

### 1. 解析运行时参数

新增一个轻量运行时参数结构，包含：

- `dataset_root`
- `config_path`
- `output_root`
- `debug`
- `golden_root`

解析完成后立即做参数合法性检查：

- `dataset_root` 和 `config_path` 必须存在。
- 如果 `output_root` 为空，则直接赋值为 `dataset_root`。
- 如果 `debug=true`，则 `golden_root` 必须非空。
- 如果 `debug=false`，则不允许传 `golden_root`。

### 2. 读取配置并注入运行时根路径

`LoadPipelineConfig()` 不再从 JSON 读取 `dataset_root`、`output_root`、`golden_root`，而是只解析静态配置内容。

随后由入口把运行时参数注入 `PipelineConfig`：

```cpp
config.dataset_root = args.dataset_root;
config.output_root = args.output_root;
config.paths.dataset_root = config.dataset_root;
```

`golden_root` 不进入静态配置解析逻辑，只在 debug 校验阶段单独使用。

## 主流程行为

主流程顺序保持现有 RT024 逻辑：

1. 解析 CLI 参数。
2. 读取 RT024 配置文件。
3. 将 CLI 提供的 `dataset_root`、`output_root` 注入 `PipelineConfig`。
4. 运行顶层 pipeline。
5. 如果未开启 `--debug`，流程结束，返回成功或运行时错误。
6. 如果开启 `--debug`，调用 `VerifyRt024Outputs(golden_root, output_root)` 做真值对比。

行为变化点只有一个：真值对比从“默认执行”调整为“仅 debug 模式执行”。

## 脚本行为

`image_virtual.bash` 改为一个显式参数包装器，不再内置固定 `config.json` 路径，也不再把运行路径写死在脚本正文里。

脚本职责：

1. 解析 `--dataset_root`、`--config_path`、`--output_root`、`--debug`、`--golden_root`。
2. 在脚本层先检查 `--dataset_root` 和 `--config_path` 是否提供。
3. 如果未提供 `--output_root`，则在脚本内把它设为 `dataset_root`。
4. 如果开启 `--debug`，则要求同时提供 `--golden_root`。
5. 如果只提供 `--golden_root` 但未开启 `--debug`，脚本报错退出。
6. 把参数原样透传给 `virtual_camera_tool`。

脚本和程序都保留参数校验，避免直接调用二进制时出现行为不一致。

## 错误处理

错误处理分两层：

### 脚本层

- 参数缺失时直接打印用法和错误原因。
- 参数组合非法时直接退出，避免进入程序主体。

### 程序层

- 即使绕过脚本直接执行二进制，也执行同样的参数校验。
- 配置文件缺字段、JSON 格式错误、运行阶段异常继续沿用现有异常和退出码风格。
- debug 验证失败时，沿用当前 `VerifyRt024Outputs()` 的失败输出和非 0 返回码。

## 测试方案

### 配置解析测试

- 配置文件不再要求 `dataset_root`、`output_root`、`golden_root`。
- 现有目录相对路径、任务数组和并行度字段继续正确解析。
- 并行度默认值仍为 1。

### CLI/入口测试

- 缺少 `--dataset_root` 时应报错。
- 缺少 `--config_path` 时应报错。
- 传 `--debug` 但未传 `--golden_root` 时应报错。
- 传 `--golden_root` 但未传 `--debug` 时应报错。
- 未传 `--output_root` 时，实际使用的 `output_root` 应等于 `dataset_root`。
- 传入 `--output_root` 时，输出路径应使用显式值。

### 主流程行为测试

- 默认模式下不触发 `VerifyRt024Outputs()`。
- debug 模式下触发 `VerifyRt024Outputs(golden_root, output_root)`。
- `output_root != dataset_root` 时，去畸变输出、虚拟相机输出、bin 输出都正确落到指定目录。

### 脚本测试

- 脚本缺少 `--dataset_root` 时应失败。
- 脚本缺少 `--config_path` 时应失败。
- 脚本未传 `--output_root` 时应把其补成 `dataset_root`。
- 脚本在 `--debug` 和 `--golden_root` 组合非法时应失败。

## 风险与处理

- 现有测试和示例配置引用了配置文件中的 `dataset_root`、`output_root`、`golden_root`，改造后需要同步更新，否则测试会因旧字段假设失败。
- 脚本和程序都做参数校验会有少量重复逻辑，但这是有意保留，用来保证直接执行二进制时的安全性。
- 默认不做真值对比会改变旧的执行习惯，因此 README、示例命令和脚本帮助信息必须同步更新，避免误以为运行结束就代表已经完成 golden 校验。

## 迁移步骤

1. 调整程序入口参数解析，新增运行时参数结构。
2. 修改 `LoadPipelineConfig()`，移除对 `dataset_root`、`output_root`、`golden_root` 的 JSON 依赖。
3. 更新 `PipelineConfig` 使用方式，使根路径由 CLI 注入。
4. 修改 `image_virtual.bash` 为显式参数脚本。
5. 更新 RT024 配置样例，移除三个根路径字段。
6. 补充和修正单元测试。
7. 更新 README 或脚本帮助信息，说明新的运行方式和 debug 语义。
