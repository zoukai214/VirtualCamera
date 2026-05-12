# RT024 Data Pipeline Design

## 背景

目标是在 `feat/data-pipeline` 分支上，参考 `/workspace/gen_vc_bin_lib_test` 的处理方式，将 `process_virtual_camera` 和 `process_undistort` 移植到当前项目中。

本次移植明确不要求兼容当前分支之前的命令行接口与配置格式，可以新增配置文件，并以新的单入口流程替代当前的 `generate-verify` / `generate-4v` 主流程。

验收数据集固定为 `/workspace/GACRT024_1754812994`：

- 输入标定目录：`/workspace/GACRT024_1754812994/calib_extract`
- 输入图片目录：`/workspace/GACRT024_1754812994/image_raw`
- 真值输出目录：
  - `calib_undistortion`
  - `calib_virtual_camera`
  - `image_undistortion`
  - `image_virtual_camera`
  - `vc_gdcbin_dir_path`

## 目标

实现一个新的配置驱动数据处理工具，完成以下两类流程：

1. `process_undistort`
2. `process_virtual_camera`

工具需要输出：

- 去畸变参数 JSON
- 虚拟相机参数 JSON
- 虚拟相机映射表 BIN
- 去畸变图像
- 虚拟相机图像

当前自动化验证范围为：

- `vc_gdcbin_dir_path` 中的虚拟相机映射表与真值逐字节一致
- `calib_undistortion` 中的参数与真值一致
- `calib_virtual_camera` 中的参数与真值一致

图像当前不做自动比对与自动测试，但运行时必须产出对应目录和文件。

## 非目标

- 不兼容旧 CLI：`generate-verify`、`generate-4v`
- 不兼容旧 Thor 配置：`config_thor*.json`
- 不实现 simulation、vc2vc、其他参考项目中的额外流程
- 当前阶段不对输出图像做像素级自动测试
- 不修改 CMake 编译选项

## 方案比较

### 方案 A：在现有 Thor/verify 流程上继续叠加

优点：

- 可复用现有 `TaskBuilder`、`MapGenerator`、`JsonWriter` 结构

缺点：

- 现有数据模型围绕 Thor 配置构建，与参考项目配置和目录语义不一致
- 需要保留较多历史兼容层，迁移后结构会更绕

### 方案 B：新建参考风格单入口流程

优点：

- 最贴近本次需求
- 配置、目录、任务语义与参考项目一致，维护更直接
- 可以复用当前项目中的底层算法与 JSON 工具，但抛弃不再需要的旧入口

缺点：

- 需要替换当前主入口与部分测试

### 方案 C：整体贴近参考项目重组代码

优点：

- 行为最接近参考项目

缺点：

- 会明显降低当前仓库的一致性
- 代码组织和可维护性最差

### 结论

采用方案 B：新建参考风格单入口流程，复用现有底层算法模块，不保留旧 CLI 兼容层。

## 运行入口

主程序改为：

```bash
./build/virtual_camera_tool <config.json>
```

运行逻辑：

1. 读取配置文件
2. 解析目录、开关与任务数组
3. 如果 `process_undistort=1`，执行去畸变流程
4. 如果 `process_virtual_camera=1`，执行虚拟相机流程
5. 执行结果校验

若任一步骤失败，进程返回非零并打印明确错误信息。

## 配置设计

新增独立配置文件，例如：

- `configs/config_rt024.json`

配置风格参考 `/workspace/gen_vc_bin_lib_test/config.json`，但只保留本次需要的字段。

### 顶层字段

```json
{
  "conf_dir_path": "calib_extract/",
  "image_dir_path": "image_raw/",
  "vc_image_dir_path": "image_virtual_camera/",
  "undistort_image_dir_path": "image_undistortion/",
  "vc_conf_dir_path": "calib_virtual_camera/",
  "undistort_conf_dir_path": "calib_undistortion/",
  "vc_gdcbin_dir_path": "vc_gdcbin_dir_path/",
  "showinfo": 1,
  "showdir": 0,
  "process_virtual_camera": 1,
  "process_undistort": 1,
  "undistort_image": 0,
  "distort_model": 0,
  "save_virtual_json": 1,
  "save_undistort_json": 1,
  "conf_type": 3,
  "virtual_camera_configs": [],
  "undistort_configs": []
}
```

### 顶层字段语义

- `conf_dir_path`：输入标定目录，相对数据根目录
- `image_dir_path`：输入原图目录，相对数据根目录
- `vc_image_dir_path`：虚拟相机图像输出目录
- `undistort_image_dir_path`：去畸变图像输出目录
- `vc_conf_dir_path`：虚拟相机参数输出目录
- `undistort_conf_dir_path`：去畸变参数输出目录
- `vc_gdcbin_dir_path`：虚拟相机映射表输出目录
- `showinfo`：输出处理进度
- `showdir`：输出关键路径
- `process_virtual_camera`：是否执行虚拟相机流程
- `process_undistort`：是否执行去畸变流程
- `undistort_image`：沿用参考项目语义，控制源图是否先按去畸变模型处理
- `distort_model`：`0` 表示 pinhole，`1` 表示 fisheye kb
- `save_virtual_json`：是否输出虚拟相机参数
- `save_undistort_json`：是否输出去畸变参数
- `conf_type`：当前按 A02/QZ 配置读取路径与 key 处理

### virtual_camera_configs

每个元素描述一个虚拟相机任务，核心字段如下：

- `desc`
- `conf_json`
- `conf_intri_key`
- `conf_extri_key`
- `image_dir`
- `save_dir`
- `camera_id`
- `image_width`
- `image_height`
- `fov`
- `undistort_image`
- `vc_mapX_name`
- `vc_mapY_name`
- `new_intrinsic`
- `new_extrinsics`

其中：

- `conf_json` 指向 `calib_extract` 下的源标定文件
- `image_dir` 指向源图子目录，例如 `front_wide/`
- `save_dir` 指向虚拟相机图输出子目录，例如 `front_wide_110/`
- `new_intrinsic` 描述输出虚拟相机分辨率与内参
- `new_extrinsics` 描述目标虚拟相机姿态

### undistort_configs

每个元素描述一个去畸变任务，核心字段如下：

- `conf_json`
- `intri_key`
- `extri_key`
- `image_dir`
- `new_intrinsic`

其中：

- `image_dir` 指向 `image_raw` 下的源图目录
- 输出目录固定写入 `image_undistortion/<image_dir>`
- `new_intrinsic` 给出去畸变后目标内参和分辨率

## 模块设计

### 1. 配置解析模块

新增配置解析层，将参考项目风格 JSON 解析成当前仓库内的强类型结构。

建议新增：

- `include/virtual_camera/pipeline_config.h`
- `src/pipeline_config.cpp`

职责：

- 读取顶层配置
- 解析目录参数
- 解析 `virtual_camera_configs`
- 解析 `undistort_configs`
- 做字段完整性校验

解析失败时抛出包含字段名的异常。

### 2. 标定加载模块

现有 `task_builder` 偏向旧 Thor 输入模型，本次应补一个新的标定读取模块，而不是继续把参考项目字段硬塞进旧 `CameraTask`。

建议新增：

- `include/virtual_camera/calibration_loader.h`
- `src/calibration_loader.cpp`

职责：

- 从 `calib_extract/<conf_json>` 读取原始内参、畸变参数、外参
- 支持通过配置中的相机 key 定位对应节点
- 根据 `distort_model` 决定畸变数组长度
- 返回统一的 `CalibrationParam`

### 3. 去畸变执行器

建议新增：

- `include/virtual_camera/undistort_processor.h`
- `src/undistort_processor.cpp`

职责：

- 基于配置任务构造去畸变映射
- 输出去畸变参数到 `calib_undistortion`
- 批量读取 `image_raw/<image_dir>` 图像并 remap
- 输出图像到 `image_undistortion/<image_dir>`

输出要求：

- 参数 JSON 与真值字段一致
- 图像目录存在且包含处理结果

### 4. 虚拟相机执行器

建议新增：

- `include/virtual_camera/virtual_camera_processor.h`
- `src/virtual_camera_processor.cpp`

职责：

- 基于配置任务构造虚拟相机映射
- 输出映射表到 `vc_gdcbin_dir_path`
- 输出虚拟相机参数到 `calib_virtual_camera`
- 批量读取 `image_raw/<image_dir>` 图像并 remap
- 输出图像到 `image_virtual_camera/<save_dir>`

输出要求：

- 映射表与真值字节级一致
- 参数 JSON 与真值字段一致
- 图像目录存在且包含处理结果

### 5. 校验模块

当前 `verifier` 面向旧输出布局，需要按本次数据集补充新的校验入口。

建议：

- 保留二进制逐字节比较能力
- 扩展 JSON 关键字段比对能力
- 不对图像内容做比较

校验范围：

1. `vc_gdcbin_dir_path`
2. `calib_undistortion`
3. `calib_virtual_camera`

## 数据流

### process_undistort

1. 读取去畸变任务配置
2. 读取对应源标定
3. 生成去畸变 map
4. 导出去畸变参数 JSON
5. 遍历对应原始图片目录
6. 对每张图片执行 `remap`
7. 写入 `image_undistortion/<camera_dir>`

### process_virtual_camera

1. 读取虚拟相机任务配置
2. 读取对应源标定
3. 生成虚拟相机 map
4. 导出映射表 BIN
5. 导出虚拟相机参数 JSON
6. 遍历对应原始图片目录
7. 对每张图片执行 `remap`
8. 写入 `image_virtual_camera/<save_dir>`

## 错误处理

出现以下情况时直接失败：

- 配置文件不存在或 JSON 非法
- 必填字段缺失
- 标定文件不存在
- 配置中的 key 在标定 JSON 中找不到
- 输入图片目录不存在
- OpenCV 读图失败
- remap 失败
- 输出文件写入失败
- 验证失败

错误信息要求包含：

- 当前处理阶段
- 当前相机或任务描述
- 失败文件路径

## 测试设计

根据 AGENTS 约束，新功能必须有对应单元测试。本次按 TDD 实施。

### 单元测试

新增测试应覆盖：

1. 配置解析
2. 标定加载
3. JSON 输出
4. 校验逻辑

建议新增：

- `tests/test_pipeline_config.cpp`
- `tests/test_calibration_loader.cpp`
- `tests/test_pipeline_verifier.cpp`

覆盖点：

- 能正确解析新配置
- 缺字段时返回明确错误
- 能从 `calib_extract` 正确取到内外参与畸变参数
- 生成的参数 JSON 关键字段符合真值格式
- 映射表校验可以发现字节差异

### 集成测试

增加一条以 `/workspace/GACRT024_1754812994` 为基准的集成验证路径。

目标：

- 执行新主入口
- 输出到临时目录
- 对比：
  - `vc_gdcbin_dir_path`
  - `calib_undistortion`
  - `calib_virtual_camera`

当前不纳入自动对比：

- `image_undistortion`
- `image_virtual_camera`

但集成运行必须确认这两个目录被正确生成且包含结果文件。

## 实施顺序

1. 新配置结构与解析测试
2. 新配置解析实现
3. 标定加载测试
4. 标定加载实现
5. 去畸变参数输出测试
6. 去畸变流程实现
7. 虚拟相机参数与 bin 校验测试
8. 虚拟相机流程实现
9. 主入口替换
10. 集成验证与清理

## 风险与注意事项

### 标定 JSON 结构风险

当前项目已有 JSON 读取逻辑主要面向旧 Thor 数据结构，而 `calib_extract` 使用的是另一套 key 组织方式。本次不要假设两者完全一致，必须以数据集中的真实字段为准。

### 图像数量较大

`image_raw` 下图像较多，集成运行时间会明显高于现有纯 bin/json 流程。实现时应避免重复解析标定和重复创建映射表。

### 路径语义

参考项目中多个路径字段是相对根目录拼接的，本次必须统一成“数据根目录 + 相对路径”的规则，避免混用当前仓库路径和数据集路径。

### 旧测试影响

由于本次不兼容旧 CLI，当前围绕 `generate-verify` 或 Thor 配置假设编写的测试需要同步调整或替换。

## 预期结果

完成后，用户可以通过新的 `config_rt024.json` 直接驱动整条流程，产出：

- `calib_undistortion`
- `calib_virtual_camera`
- `image_undistortion`
- `image_virtual_camera`
- `vc_gdcbin_dir_path`

其中：

- 虚拟相机映射表与真值一致
- 去畸变参数与虚拟相机参数与真值一致
- 图像按预期输出到对应目录
