# X86 Thor Bin 验证优化设计

## 背景

本次优化目标是在 `/workspace/VirtualCamera` 中生成一个优化后的虚拟相机项目。原始项目 `/workspace/icv_vc_bin_lib` 只作为参考来源，不再修改其中任何文件，也不在其中执行 git 操作。

参考项目当前使用 `configs/config_thor.json` 描述 L022 车型 7v 配置下的任务：

- `calib/gdc/`：虚拟相机映射表 bin。
- `calib/gdc_intri/`：去畸变映射表 bin。
- `calib/virtual/`：虚拟相机内外参 json。

本次只完成 x86 平台，不处理 Thor 交叉编译平台。测试数据固定为 `/workspace/L022/cfg/7v/`，其中已有的 `calib/gdc`、`calib/gdc_intri`、`calib/virtual` 作为金标准。

## 工作目录约束

1. 所有新增项目文件、代码、脚本、文档都写入 `/workspace/VirtualCamera`。
2. `/workspace/icv_vc_bin_lib` 不再做任何文件修改。
3. 后续所有 git 操作都只在 `/workspace/VirtualCamera` 中执行。
4. `/workspace/L022/cfg/7v/` 只作为输入和金标准读取，不覆盖其中现有输出。

## 验收标准

本次采用用户确认的验收标准：

1. `gdc/*.bin` 必须与金标准逐字节一致。
2. `gdc_intri/*.bin` 必须与金标准逐字节一致。
3. `virtual/**/*.json` 做关键标定字段级一致，不要求逐字节一致。
4. 输出文件集合必须与 `config_thor.json` 对应任务一致。
5. 只验证 x86 构建和运行链路。

JSON 字段级比较覆盖：

- 内参矩阵。
- 外参矩阵中的旋转和平移。
- 图像尺寸字段。
- 畸变系数字段。

JSON 比较忽略：

- `time_lag` 等运行时生成时间字段。
- JSON 字段顺序、缩进、换行等格式差异。

## 非目标

1. 不完成 Thor/aarch64 交叉编译适配。
2. 不修改原始项目 `/workspace/icv_vc_bin_lib`。
3. 不删除现有文件或覆盖 `/workspace/L022/cfg/7v/` 下的金标准输出。
4. 不重写完整投影算法；只有当 bin 对比证明算法差异时，才做最小范围修正。
5. 不引入全新的运行配置体系，先保持 `config_thor.json` 作为任务来源。

## 推荐方案

采用“在 `/workspace/VirtualCamera` 中创建优化后 x86 项目，保留旧配置入口，新增 Thor 任务执行与验证层”的方案。

该方案把参考项目的必要代码迁移到新仓库中，再以 `config_thor.json` 为业务描述来源，将其中的 `virtual_camera_configs` 和 `virtual_init_camera_config` 展开为统一任务列表。执行层负责生成输出，验证层负责与金标准比较。

## 架构设计

### 项目组织

新项目位于 `/workspace/VirtualCamera`，建议结构：

- `CMakeLists.txt`：x86 构建入口。
- `app/`：虚拟相机任务执行、算法封装、验证入口。
- `common/`：JSON、路径、文件比较等公共工具。
- `configs/config_thor.json`：从参考项目迁移来的 Thor 任务配置。
- `scripts/`：构建、运行、验证脚本。
- `docs/superpowers/specs/`：设计规格文档。
- `output_verify/`：本地生成结果目录，不提交产物。

### 运行入口

x86 运行使用：

- 输入根目录：`/workspace/L022/cfg/7v/`
- 配置文件：`/workspace/VirtualCamera/configs/config_thor.json`
- 输出根目录：`/workspace/VirtualCamera/output_verify/7v/`

输出目录不覆盖金标准，避免破坏测试数据。

### 任务展开

从 `config_thor.json` 展开两类任务：

1. `virtual_camera_configs`
   - 生成 1024x512 虚拟相机 `gdc/*.bin`。
   - 为每个唯一真实相机生成一次 `gdc_intri/*.bin`。
   - 生成对应 `virtual/**/*.json`。

2. `virtual_init_camera_config`
   - 生成 1920x1088 resize 变体 `gdc/*_1080.bin`。
   - 生成对应 `virtual/**/*.json`。

任务对象至少包含：

- 任务类型：`virtual`、`undistort`、`resize`。
- 源内参 json 路径。
- 源外参 json 路径。
- 输出 bin 路径。
- 输出 virtual json 路径。
- 源图像尺寸。
- 目标图像尺寸。
- 目标 FOV。
- 目标 yaw/pitch/roll。
- camera_id。

### 算法执行

保留参考项目中的算法思想，并在新项目中整理边界：

- 虚拟相机映射负责生成 `gdc/*.bin`。
- 去畸变映射负责生成 `gdc_intri/*.bin`。
- resize 映射负责生成 `gdc/*_1080.bin`。
- bin 写出统一处理越界点置零和二进制输出。

实现时先在新项目中复现现有算法并对比金标准。若 bin 不一致，再根据差异定位到具体任务和算法路径，只修改导致差异的最小代码块。

### 输出写入

输出写入遵循以下规则：

1. 所有产物写到 `output_verify/7v/calib/...`。
2. `gdc` 和 `gdc_intri` 下只生成目标 `.bin` 文件。
3. `.checkcode` 可以作为附加验证产物生成，但不参与核心 bin 对比。
4. `virtual` json 使用模板字段生成，但验证时只比较关键标定字段。

### 验证层

新增验证工具或测试入口，职责：

1. 枚举金标准 `calib/gdc` 和输出 `calib/gdc` 的 `.bin` 文件集合。
2. 枚举金标准 `calib/gdc_intri` 和输出 `calib/gdc_intri` 的 `.bin` 文件集合。
3. 对每个 `.bin` 文件做逐字节比较。
4. 对 `virtual` json 做字段级比较。
5. 输出清晰失败信息：
   - 缺少文件。
   - 多出文件。
   - 字节内容不同的文件。
   - JSON 关键字段不同的位置和值。

## 测试设计

至少增加以下验证：

1. x86 构建验证：运行新项目构建脚本。
2. Thor 配置生成验证：使用 `/workspace/L022/cfg/7v/` 和新项目内 `configs/config_thor.json` 生成到 `output_verify/7v/`。
3. `gdc` bin 字节级比较。
4. `gdc_intri` bin 字节级比较。
5. `virtual` json 字段级比较。

若新项目不引入单元测试框架，优先增加可执行验证程序或脚本，作为本次回归测试入口。

## 风险与处理

### 金标准被覆盖

风险：旧脚本可能移动或覆盖数据目录下的输出。

处理：新项目的验证流程输出到 `/workspace/VirtualCamera/output_verify/7v/`，不修改 `/workspace/L022/cfg/7v/` 下的金标准。

### JSON 无法字节级一致

风险：运行时生成时间和格式化输出可能与金标准不同。

处理：本次验收已明确 JSON 字段级一致，忽略时间和格式。若后续需要 JSON 字节级一致，再单独做确定性 JSON 输出设计。

### bin 浮点差异

风险：OpenCV 版本、x86 编译选项或算法细节可能导致浮点结果不同。

处理：验收要求是逐字节一致，因此一旦出现差异，先定位具体文件和首个不同字节，再反推到对应 map 生成路径做最小修正。

## 交付物

1. `/workspace/VirtualCamera` 中的优化后 x86 项目。
2. 输出目录 `output_verify/7v/calib/{gdc,gdc_intri,virtual}`。
3. bin 逐字节验证工具或脚本。
4. virtual JSON 字段级验证工具或脚本。
5. 针对验证发现问题的最小算法或流程修正。
6. 推送到 `https://github.com/zoukai214/VirtualCamera` 的代码提交。
