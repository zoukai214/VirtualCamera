# Thor 多配置文件生成设计

## 背景

`configs/config_thor.json` 已支持通过 `LoadThorConfig` 兼容读取旧完整 JSON 和新 include 主配置。当前需要基于现有完整配置生成一组多文件配置，并使用 `/workspace/L022/cfg/7v` 数据验证新入口可用。

## 目标

1. 保留 `configs/config_thor.json` 不变，作为旧格式兼容基准。
2. 新增 `configs/config_thor_include.json` 作为多配置入口。
3. 新增 `configs/thor/common.json`、`configs/thor/virtual_camera_configs.json`、`configs/thor/virtual_init_camera_config.json`。
4. 使用新入口和 `/workspace/L022/cfg/7v` 执行生成验证。

## 文件拆分

`configs/config_thor_include.json` 只包含 include 列表：

```json
{
  "include": [
    "thor/common.json",
    "thor/virtual_camera_configs.json",
    "thor/virtual_init_camera_config.json"
  ]
}
```

`common.json` 保存除 `virtual_camera_configs` 和 `virtual_init_camera_config` 以外的顶层字段。

`virtual_camera_configs.json` 保存原 `virtual_camera_configs` 数组。

`virtual_init_camera_config.json` 保存原 `virtual_init_camera_config` 数组。

## 验证

执行：

```bash
./scripts/build.sh
./build/test_task_builder
./build/test_verifier
./build/virtual_camera_tool generate-verify /workspace/L022/cfg/7v configs/config_thor_include.json output_verify/7v_include
```

期望输出验证通过，并且不修改旧配置入口。
