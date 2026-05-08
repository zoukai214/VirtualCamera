# Thor 配置多文件兼容读取设计

## 背景

当前 `configs/config_thor.json` 同时包含全局路径、流程开关、虚拟相机任务、resize 初始化任务等内容。文件体积较大，且相机任务中存在较多重复字段，后续维护容易出现遗漏或不一致。

现有命令行入口仍然只接收一个配置路径：

```bash
./build/virtual_camera_tool generate-verify <input_root> <config_path> <output_root>
```

为了降低迁移风险，本次设计保留旧格式读取能力，并新增多文件配置入口。

## 目标

1. 继续兼容现有完整 JSON 配置文件，`configs/config_thor.json` 不拆分时行为不变。
2. 支持一个主配置文件引用多个子配置文件，便于按全局配置、虚拟相机任务、resize 任务分开维护。
3. 对 `BuildThorTasks` 的输入结构保持兼容，降低对任务构建、地图生成、JSON 写出和验证流程的影响。
4. 新增单元测试覆盖旧格式与 include 格式读取结果一致。

## 非目标

1. 不修改 `CMakeLists.txt` 编译选项。
2. 不删除现有 `configs/config_thor.json`。
3. 不在本次直接引入 YAML、ROS param server 或外部配置库。
4. 不改变现有输出 bin/json 的生成规则。

## 配置格式

旧格式继续保持完整配置：

```json
{
  "virtual_camera_configs": [],
  "virtual_init_camera_config": []
}
```

新增 include 主配置格式：

```json
{
  "include": [
    "thor/common.json",
    "thor/virtual_camera_configs.json",
    "thor/virtual_init_camera_config.json"
  ]
}
```

include 路径相对于主配置文件所在目录解析。例如主配置为 `configs/config_thor.json`，则 `thor/common.json` 解析为 `configs/thor/common.json`。

子配置文件使用与旧格式相同的 key。加载器按顺序合并所有子配置：

```json
{
  "virtual_camera_configs": [
    {}
  ]
}
```

```json
{
  "virtual_init_camera_config": [
    {}
  ]
}
```

如果多个子配置包含同名数组 key，数组追加合并；如果包含同名普通 key，后加载文件覆盖先加载文件。这样可以在 `common.json` 中放默认开关，也允许后续文件局部覆盖。

## 架构

新增配置加载接口：

```cpp
nlohmann::json LoadThorConfig(const std::string& config_path);
```

职责：

1. 读取入口 JSON。
2. 如果入口没有 `include` 字段，直接返回入口 JSON。
3. 如果入口存在 `include` 字段，依次读取子配置并合并。
4. 返回合并后的完整 JSON，继续传给 `BuildThorTasks`。

`src/main.cpp` 从：

```cpp
const auto config = vc::ReadJson(config_path);
```

调整为：

```cpp
const auto config = vc::LoadThorConfig(config_path);
```

`BuildThorTasks` 暂时不感知多文件来源，继续只处理合并后的 `virtual_camera_configs` 和 `virtual_init_camera_config`。

## 错误处理

1. `include` 必须是字符串数组；否则抛出明确异常。
2. 子配置路径不存在时，复用现有 `ReadJson` 文件打开错误，并带出实际解析后的路径。
3. 子配置必须是 JSON object；否则抛出明确异常。
4. include 文件不做递归 include，避免循环引用和调试复杂度。

## 测试

新增或扩展 `tests/test_task_builder.cpp`：

1. 旧格式：读取现有 `configs/config_thor.json`，任务数量仍为 9 个 virtual、7 个 undistort、3 个 resize。
2. include 格式：测试内构造临时主配置与子配置，验证 `LoadThorConfig` 合并后能生成等价任务。
3. 错误格式：`include` 不是数组时应抛异常。

现有验证命令保持：

```bash
./scripts/build.sh
./build/test_task_builder
./build/test_verifier
```

如果本地有完整 Thor 数据，再运行：

```bash
./scripts/run_thor_verify.sh
```

## 迁移步骤

第一阶段只实现兼容读取，不拆现有配置文件。

第二阶段在确认读取稳定后，新增 `configs/thor/` 子配置示例，并把 `configs/config_thor.json` 改成 include 主配置。该阶段需要单独确认，因为会改变配置文件组织方式。
