# 7V Third-Party Migration Design

## 背景

当前项目的顶层 `CMakeLists.txt` 直接依赖两个工作区外目录：

- `/workspace/icv_vc_bin_lib/deps/x86`
- `/workspace/gen_vc_bin_lib_test/deps/x86/gen_vc_map_lib`

其中：

- `opencv`、`eigen`、`nlohmann_json` 目前通过外部目录提供
- `gen_vc_map_lib`、`gdc_bin_generator` 通过外部头文件和动态库提供
- `third_party/gdc_add_cylinder` 已经以源码方式并入当前项目，但本次不再保留它作为依赖来源

本次需求是将 7v 主流程从这些外部路径中解耦，只保留可 vendoring 的通用依赖，并将映射生成能力收敛到当前仓库内部实现。

用户额外约束如下：

- 只实现 7v 功能
- 不需要实现 4v 功能
- 去掉 4v 相关部分
- `opencv`、`eigen`、`nlohmann_json` 可以复制到 `third_party`
- `gen_vc_map_lib`、`gdc_add_cylinder` 不再作为依赖
- `gdc_bin_generator` 不再依赖，也不补实现

验收数据集固定为 `/workspace/GACRT024_1754812994`：

- 输入标定目录：`/workspace/GACRT024_1754812994/calib_extract`
- 输入图片目录：`/workspace/GACRT024_1754812994/image_raw`
- 真值输出目录：
  - `calib_undistortion`
  - `calib_virtual_camera`
  - `image_undistortion`
  - `image_virtual_camera`
  - `vc_gdcbin_dir_path`

本次真值比对范围明确为：

- `vc_gdcbin_dir_path` 中的 bin 输出必须与真值一致
- `calib_undistortion` 中的参数 JSON 必须与真值一致
- `calib_virtual_camera` 中的参数 JSON 必须与真值一致

图片输出仍需生成，但不做真值比较。

## 目标

完成一个仅面向 7v RT024 流程的仓内化实现，满足以下目标：

1. 构建时不再依赖 `/workspace/icv_vc_bin_lib` 和 `/workspace/gen_vc_bin_lib_test`
2. `opencv`、`eigen`、`nlohmann_json` 从当前仓库 `third_party` 提供
3. 去畸变和虚拟相机映射生成不再依赖 `<gen_vc_map.hpp>` 和外部动态库
4. 移除 4v 代码路径、4v 依赖和 4v 构建入口
5. 现有 7v 配置驱动流程保持可运行
6. 基于固定数据集回归时，bin 和参数 JSON 与真值一致

## 非目标

- 不实现 4v 生成功能
- 不保留 `generate-4v` 或任何 4v 兼容入口
- 不补 `gdc_bin_generator` 功能
- 不对输出图片做自动真值比较
- 不修改 `CMakeLists.txt` 中编译选项
- 不引入新的系统级安装依赖

## 方案比较

### 方案 A：继续保留外部 map 库，只把通用依赖迁到 `third_party`

优点：

- 改动最小
- 风险最低

缺点：

- 不满足“`gen_vc_map_lib` 不要依赖”的要求
- 仓库仍然依赖工作区外动态库，无法独立构建

### 方案 B：保留 7v 流程入口，重写仓内 map 生成功能

优点：

- 满足去外部 map 库依赖的目标
- 只改 7v 主链路，范围可控
- 可继续复用现有配置解析、标定读取、JSON 输出和并行执行框架

缺点：

- 需要把去畸变和虚拟相机映射逻辑收敛到当前仓库
- 需要回归验证 bin 和参数结果

### 方案 C：整体回退到更早的 7v/4v 混合结构后再重做

优点：

- 理论上可以统一整理老代码

缺点：

- 改动面过大
- 会把 4v 一并带回，不符合本次只保留 7v 的范围

### 结论

采用方案 B。

保留当前 7v 配置驱动主流程，移除 4v，替换外部 map 库为仓内实现，并将可保留的通用三方依赖复制到 `third_party`。

## 总体设计

### 1. 依赖边界

保留并 vendoring 到 `third_party` 的依赖：

- `third_party/opencv`
- `third_party/eigen`
- `third_party/nlohmann_json`

彻底移除的依赖：

- `/workspace/icv_vc_bin_lib/deps/x86`
- `/workspace/gen_vc_bin_lib_test/deps/x86/gen_vc_map_lib`
- `third_party/gdc_add_cylinder`
- `libgen_vc_map_lib.so.1.0`
- `libgdc_bin_generator.so`
- `libgdc.so`

### 2. 功能边界

保留的主流程：

- `process_undistort`
- `process_virtual_camera`
- 结果校验

移除的主流程：

- 4v 配置解析
- 4v 生成入口
- 圆柱 bin 生成
- 任何 4v 专用测试

### 3. 模块分层

#### 配置与任务层

继续使用当前 RT024 配置解析与任务编排：

- `src/pipeline_config.cpp`
- `src/calibration_loader.cpp`
- `src/task_builder.cpp`

这些模块负责：

- 解析 7v 配置
- 读取输入标定
- 构造去畸变与虚拟相机任务

#### 映射生成层

新增或重构一个项目内映射生成模块，替代外部 `gen_vc_map` 接口。

职责：

- 生成去畸变 `map_x/map_y`
- 生成虚拟相机 `map_x/map_y`
- 生成 `src2vc_map_x/map_y`
- 负责与当前输出格式兼容的 bin 或 float map 落盘

这个模块只暴露当前项目实际需要的接口，不再模拟整套外部库 API。

#### 输出层

保留现有输出职责：

- `json_writer` 负责参数 JSON
- `undistort_processor` 和 `virtual_camera_processor` 负责图片输出和文件落盘
- `verifier` 负责真值目录比较

## 算法设计

### 去畸变

当前 `src/undistort_processor.cpp` 通过外部接口：

- `gen_undis_map`
- `gen_undis_map_kb`

生成去畸变 map，再用 `cv::remap` 输出图片。

新的仓内实现保持同一处理阶段，但改为直接基于 OpenCV 生成映射：

- pinhole 模型使用 `cv::initUndistortRectifyMap`
- fisheye KB 模型使用 `cv::fisheye::initUndistortRectifyMap`

输入参数来源不变：

- 原始内参矩阵
- 畸变参数
- 图像宽高
- 新内参参数

输出仍为：

- 去畸变参数 JSON
- 去畸变图片目录

### 虚拟相机

当前 `src/virtual_camera_processor.cpp` 通过外部接口：

- `gen_vc_map`
- `gen_vc_map_kb`

生成：

- 虚拟相机 remap 用的 `map_x/map_y`
- `src2vc_map_x/map_y`

新的仓内实现采用当前仓库已有的几何计算能力为核心：

- 复用 `MapGenerator` 对虚拟相机内参和外参的构造逻辑
- 在项目内新增函数，基于标定、目标姿态和目标 FOV 直接生成 remap map
- `src2vc` 输出按当前流程需要同步生成并写盘

实现上不追求复刻外部库的 API，而是追求：

- 输出目录结构不变
- bin 与 JSON 能通过当前真值校验
- 代码只保留本项目使用到的 7v 语义

### bin 输出

当前 `vc_gdcbin_dir_path` 下由 `SaveFloatMap()` 直接写 `float` 数据。

新实现保持这一输出约定：

- map 按当前读取和比对逻辑落盘
- 输出文件名沿用配置中的 `vc_mapx_name`、`vc_mapy_name`、`src2vc_mapx_name`、`src2vc_mapy_name`

只要与真值目录逐文件一致，即视为通过。

## 文件与代码组织

### 保留并修改

- `CMakeLists.txt`
  - 改为引用 `third_party` 内的 `opencv`、`eigen`、`nlohmann_json`
  - 删除外部库绝对路径
  - 删除 4v 相关源码和依赖
- `src/undistort_processor.cpp`
  - 去掉 `<gen_vc_map.hpp>`
  - 改用仓内去畸变 map 生成函数
- `src/virtual_camera_processor.cpp`
  - 去掉 `<gen_vc_map.hpp>`
  - 改用仓内虚拟相机 map 生成函数
- `src/main.cpp`
  - 删除 4v 入口或分支
- `README.md`
  - 更新依赖说明与运行说明

### 新增

- `include/virtual_camera/remap_generator.h`
- `src/remap_generator.cpp`

建议职责：

- 统一提供 7v 所需的 map 生成接口
- 封装 pinhole 与 fisheye 两类实现分支
- 提供 src2vc map 生成与落盘辅助能力

### 删除

以下内容将从本分支中移除：

- `src/four_view_runner.cpp`
- `include/virtual_camera/four_view_runner.h`
- `tests/test_four_view_config.cpp`
- `tests/test_four_view_camera_params.cpp`
- `configs/config_4v.yaml`
- `scripts/run_4v_verify.sh`
- `third_party/gdc_add_cylinder/`

如果还有 4v 专用的构建条目、引用头文件或测试辅助，也一并清理。

## 错误处理

保持当前项目风格：

- 缺少输入文件、标定 key、输出目录创建失败、图片读取失败、map 写盘失败时抛出 `std::runtime_error`
- 错误消息带具体路径或字段名，便于直接定位

新增要求：

- 如果配置仍引用 4v 入口或 4v 资源，程序应在解析或入口分发阶段直接报错，而不是静默忽略

## 测试设计

### 单元测试

保留并更新 7v 相关测试：

- `tests/test_pipeline_config.cpp`
- `tests/test_calibration_loader.cpp`
- `tests/test_task_builder.cpp`
- `tests/test_undistort_processor.cpp`
- `tests/test_virtual_camera_processor.cpp`
- `tests/test_rt024_json_writer.cpp`
- `tests/test_rt024_verifier.cpp`

新增测试重点：

- 仓内 remap 生成模块的基础输出尺寸和类型
- pinhole/fisheye 分支下 map 生成是否成功
- `virtual_camera_processor` 不再依赖外部头文件和动态库

删除 4v 相关测试：

- `tests/test_four_view_config.cpp`
- `tests/test_four_view_camera_params.cpp`

### 回归验证

固定使用：

- 输入根目录：`/workspace/GACRT024_1754812994`
- 配置文件：`configs/config_rt024.json` 或等价 RT024 配置

回归关注项：

1. `calib_undistortion` 参数 JSON 与真值一致
2. `calib_virtual_camera` 参数 JSON 与真值一致
3. `vc_gdcbin_dir_path` 全部输出文件与真值一致

图片要求：

- `image_undistortion`
- `image_virtual_camera`

只要求成功产出，不做真值比对。

## 风险与处理

### 风险 1：仓内映射实现与外部库结果存在偏差

处理：

- 以固定数据集的 bin 和 JSON 真值为唯一验收标准
- 逐步替换 `undistort` 和 `virtual_camera`，先保证 JSON，再收敛 bin

### 风险 2：4v 删除后残留构建引用

处理：

- 先做构建入口和测试入口清理
- 再编译一次确认无悬挂源文件和符号

### 风险 3：vendored 三方依赖目录较大

处理：

- 只复制当前构建实际需要的头文件和库目录
- 不改变编译选项，只调整 include 和 link 路径

## 验收标准

满足以下条件视为完成：

1. 项目构建时不再引用 `/workspace/icv_vc_bin_lib` 和 `/workspace/gen_vc_bin_lib_test`
2. 代码中不再包含 `<gen_vc_map.hpp>`、`gdc_add_cylinder` 或 4v 相关入口依赖
3. 7v 主流程可在当前仓库内独立构建和运行
4. 回归运行后：
   - `vc_gdcbin_dir_path` 与真值一致
   - `calib_undistortion` 与真值一致
   - `calib_virtual_camera` 与真值一致
5. 输出图片目录成功生成

