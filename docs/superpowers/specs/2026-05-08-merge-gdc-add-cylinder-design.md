# 合并 gdc_add_cylinder 设计

## 背景

`VirtualCamera` 当前提供 7V Thor 配置的虚拟相机映射表生成和校验，入口为
`virtual_camera_tool generate-verify <input_root> <config_path> <output_root>`。

`/workspace/gdc_add_cylinder` 是独立 CMake 工具，使用 YAML 配置生成 4V 环视相机映射表，并额外支持柱面展开映射表。测试输入位于 `/workspace/L022/cfg/4v/`，现有 golden 输出位于 `/workspace/L022/cfg/4v/maps`。

本次目标是在 `VirtualCamera` 中合并两种能力，同时保留 4V 工具原有 YAML 配置方式和配置文件语义，并验证新 4V 生成结果及既有 7V 生成结果。

## 目标

- 保留现有 7V Thor 命令和行为。
- 在 `virtual_camera_tool` 中新增 4V YAML 生成命令。
- 保留 `gdc_add_cylinder` 的 YAML 配置格式，包括 `input`、`output`、`stitching`、`image`、`cylinder` 字段。
- 4V 输出文件名和二进制格式与原工具兼容。
- 测试覆盖 4V 生成正确性和 7V 回归正确性。

## 非目标

- 不把 7V 配置改造成 YAML。
- 不重构 7V `TaskBuilder`、`MapGenerator`、`Verifier` 的行为。
- 不删除 `/workspace/gdc_add_cylinder` 或 `/workspace/L022` 中的任何文件。
- 不修改现有 CMake 编译选项。
- 不引入全局 pip 包或 Python 运行时依赖。

## 命令行设计

保留现有命令：

```bash
./virtual_camera_tool generate-verify <input_root> <config_path> <output_root>
```

新增 4V 命令：

```bash
./virtual_camera_tool generate-4v <config.yaml>
```

`generate-4v` 读取 YAML 中定义的输入和输出路径。绝对路径按原样使用；相对路径按配置文件所在目录解析。

## 模块设计

新增 4V 相关模块并纳入 `virtual_camera_core`：

- `four_view_config_loader`
  - 读取原 YAML 配置。
  - 保留默认值和字段命名。
  - 负责路径解析和必要字段校验。

- `four_view_camera_params_loader`
  - 读取 `fisheye_cam_param.json`。
  - 支持 `front`、`left`、`right`、`back`、`rear`。
  - 保留原相机顺序：`left=0`、`front=1`、`right=2`、`rear/back=3`。

- `four_view_map_generator`
  - 迁移 4V BEV 环视映射表生成逻辑。
  - 生成 `map_x`、`map_y`、`mask`、`weight`。
  - 输出文件与原工具保持兼容。

- `cylinder_map_generator`
  - 迁移柱面展开核心算法。
  - 生成 `gdc_front.bin`、`gdc_left.bin`、`gdc_right.bin`、`gdc_rear.bin`。
  - 保留柱面相机 ID 语义：`0=front`、`1=left`、`2=right`、`3=rear`。

- `four_view_runner`
  - 串联配置读取、相机参数读取、映射表生成、bin 输出。
  - 作为 `main.cpp` 的 4V 命令调用入口。

现有 7V 模块保持原接口和行为，只在 `main.cpp` 中增加命令分支。

## 数据流

4V 数据流：

1. `generate-4v` 接收 YAML 配置路径。
2. `four_view_config_loader` 读取并校验配置。
3. `four_view_camera_params_loader` 读取 YAML 指定的 `camera_params_file`。
4. `four_view_map_generator` 根据 `stitching`、`image` 和相机参数生成 BEV 映射表。
5. bin writer 写入 YAML 指定的 `output.camera_maps_file`。
6. 如果 `cylinder.enabled=true`，`cylinder_map_generator` 生成柱面映射表并写入 `cylinder.output_dir`。

7V 数据流维持现状：

1. `generate-verify` 构建 Thor 任务。
2. 生成 `gdc`、`gdc_intri`、`virtual` 输出。
3. 按现有逻辑归一化 golden 文件。
4. 运行 `VerifyOutputs`。

## 输出兼容性

4V BEV 输出目录继续包含：

- `front_map_x.bin`
- `front_map_y.bin`
- `front_mask.bin`
- `front_weight.bin`
- `rear_map_x.bin`
- `rear_map_y.bin`
- `rear_mask.bin`
- `rear_weight.bin`
- `left_map_x.bin`
- `left_map_y.bin`
- `left_mask.bin`
- `left_weight.bin`
- `right_map_x.bin`
- `right_map_y.bin`
- `right_mask.bin`
- `right_weight.bin`
- `metadata.txt`

柱面输出目录继续包含：

- `gdc_front.bin`
- `gdc_left.bin`
- `gdc_right.bin`
- `gdc_rear.bin`

如果 golden 目录没有柱面输出文件，测试只验证 BEV 输出；柱面 golden 位置需要在实现前从现有数据中确认。

## 构建设计

- 将 4V 源文件加入 `virtual_camera_core`。
- 链接 `yaml-cpp`，用于保留 YAML 配置方式。
- 继续使用项目现有 OpenCV、Eigen、nlohmann_json 依赖。
- 不修改 `CMAKE_CXX_FLAGS` 等现有编译选项。

## 错误处理

- YAML 文件不存在或解析失败时返回非 0。
- 必要配置字段缺失时返回非 0，并打印字段名。
- JSON 缺少必要相机或相机参数时返回非 0。
- 输出目录创建失败或 bin 写入失败时返回非 0。
- 4V 命令不删除输入目录或 golden 文件。

## 测试设计

新增 4V 测试脚本或测试目标：

1. 使用 `/workspace/L022/cfg/4v/` 的配置和输入数据。
2. 将输出重定向到临时目录，避免污染 golden。
3. 生成 4V BEV 映射表。
4. 与 `/workspace/L022/cfg/4v/maps` 中的 golden 文件做 byte-for-byte 比对。
5. 如果能确认柱面 golden 文件位置，则额外比对 `gdc_*.bin`。

保留并运行现有 7V 回归：

```bash
bash scripts/run_thor_verify.sh
```

最终验收至少包括：

```bash
bash scripts/build.sh
bash scripts/run_thor_verify.sh
<4v 验证命令或脚本>
```

## 验收标准

- `virtual_camera_tool generate-verify ...` 仍通过现有 7V 验证。
- `virtual_camera_tool generate-4v <config.yaml>` 能按原 YAML 配置生成 4V 输出。
- 4V BEV 输出与 `/workspace/L022/cfg/4v/maps` golden 文件 byte-for-byte 一致。
- 若柱面 golden 已确认，柱面输出也 byte-for-byte 一致。
- 新增单元测试或脚本测试可在本项目内重复运行。
