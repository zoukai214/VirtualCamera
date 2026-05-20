# Rectify Virtual Camera Packaging Design

## 背景

当前项目已经能在仓库内完成构建，主执行入口为：

```bash
./build/virtual_camera_tool configs/config_rt024_parallel_run.json
```

当前仓库的运行特点如下：

- 可执行文件由顶层 `CMakeLists.txt` 生成到 `build/virtual_camera_tool`
- 默认并行配置文件位于 `configs/config_rt024_parallel_run.json`
- 项目当前不再依赖外部 map 动态库
- 下游期望拿到一个独立目录直接运行，而不是理解当前源码树或重新组织路径

参考目录为 `/workspace/rectify_virtual_camera`。用户希望本项目也输出一个同层级风格的包，但本次包目录固定输出到：

```text
build/rectify_virtual_camera
```

并且有两个额外约束：

1. 本次打包默认面向“多线程版本”，即默认配置必须使用并行配置
2. `image_virtual.bash` 中不能带有 `build/...` 路径，因为后续 `rectify_virtual_camera` 目录会单独发给下游，脚本必须只依赖包内相对路径

## 目标

本次打包设计需要满足以下目标：

1. 在当前项目构建完成后，可以通过 CMake 安装逻辑输出一个独立运行包到 `build/rectify_virtual_camera`
2. 包目录层级尽量对齐 `/workspace/rectify_virtual_camera`
3. 包内默认配置固定为并行版本，即 `configs/config_rt024_parallel_run.json`
4. 包内提供一个下游直接运行的 `image_virtual.bash`
5. 启动脚本不能引用源码树中的 `build/...` 路径，只能依赖包内文件
6. 不修改当前核心处理逻辑，不修改 `CMakeLists.txt` 编译选项
7. 新增对应测试，验证打包产物结构和默认配置选择

## 非目标

- 不改 `virtual_camera_tool` 的业务逻辑
- 不改当前并行算法、任务执行逻辑或输出格式
- 不引入新的系统安装依赖
- 不把当前项目改造成完整 SDK 目录结构
- 不新增面向下游的头文件发布能力
- 不修改 `catkin_make` 或 ROS 相关构建选项
- 不自动删除已有文件或目录

## 方案比较

### 方案 A：使用 `install()` 规则直接生成独立运行包

做法：

- 在顶层 `CMakeLists.txt` 中新增安装规则
- 将可执行文件、并行配置和启动脚本安装到 `build/rectify_virtual_camera`
- 通过 `cmake --install` 生成最终包

优点：

- 与参考项目的 `install` 用法一致
- 打包逻辑集中在 CMake，后续维护成本低
- 产物目录结构可预测，适合持续扩展

缺点：

- 需要在当前项目里显式设计安装布局
- 需要补一个安装后验证测试

### 方案 B：增加 `add_custom_target()`，通过拷贝命令手工打包

做法：

- 新增一个 `package` target
- 在 target 中手工复制文件到 `build/rectify_virtual_camera`

优点：

- 实现直观
- 上手快

缺点：

- 路径逻辑容易散在命令里
- 维护性和可读性都差于 `install()`
- 与参考项目思路不一致

### 方案 C：保留 `install()`，再额外加一层 shell 打包脚本

做法：

- CMake 提供基础安装规则
- 再由 shell 脚本调用安装并做二次整理

优点：

- 可为下游提供更固定的调用入口

缺点：

- 对当前需求来说多一层封装
- 增加额外维护面，不是本次必须内容

## 结论

采用方案 A。

本次以 CMake 原生 `install()` 作为唯一打包路径，把当前编译产物和并行配置安装到 `build/rectify_virtual_camera`，同时新增一个只依赖包内相对路径的 `image_virtual.bash` 作为下游运行入口。

## 包目录设计

最终包目录设计为：

```text
build/rectify_virtual_camera/
├── config.json
├── image_virtual.bash
├── virtual_camera_tool
└── output/
    └── x86/
        └── lib/
```

说明如下：

- `virtual_camera_tool`
  - 直接放在包根目录
  - 避免脚本中出现 `build/...` 路径
  - 便于下游直接执行或被脚本调用

- `config.json`
  - 直接放在包根目录
  - 内容来自 `configs/config_rt024_parallel_run.json`
  - 作为默认并行运行配置

- `image_virtual.bash`
  - 直接放在包根目录
  - 启动时切换到脚本所在目录
  - 只调用包内的 `./virtual_camera_tool ./config.json`

- `output/x86/lib`
  - 保留参考目录同层级风格
  - 当前项目若无必须下发的运行时动态库，该目录可以为空目录
  - 如果后续验证发现存在实际运行时 `.so` 依赖，再在同一目录补充安装

本设计有意不保留包内 `build/virtual_camera_tool` 的布局，因为用户已明确要求脚本中不要带 `build` 路径，且该包后续会单独提供给下游，包内入口应尽量扁平。

## 安装内容设计

### 可执行文件

安装对象：

- CMake 目标 `virtual_camera_tool`

安装位置：

```text
build/rectify_virtual_camera/virtual_camera_tool
```

要求：

- 使用当前构建产物，不改目标名
- 保持现有链接方式不变

### 默认配置

安装来源：

- `configs/config_rt024_parallel_run.json`

安装位置：

```text
build/rectify_virtual_camera/config.json
```

要求：

- 文件名统一为 `config.json`
- 包内默认只有并行版本配置作为启动入口
- 不要求同时复制串行配置

### 启动脚本

新增文件：

- `image_virtual.bash`

安装位置：

```text
build/rectify_virtual_camera/image_virtual.bash
```

脚本行为：

1. 定位脚本自身所在目录
2. 切换到该目录
3. 调用：

```bash
./virtual_camera_tool ./config.json
```

要求：

- 不引用源码树路径
- 不引用 `build/...` 路径
- 不依赖外部环境变量
- 允许从任意当前工作目录启动

### 兼容目录

创建目录：

```text
build/rectify_virtual_camera/output/x86/lib
```

目的：

- 对齐参考包的目录风格
- 为后续补充运行时动态库留出固定位置

## CMake 设计

本次只修改顶层 `CMakeLists.txt` 的安装逻辑，不修改编译选项。

设计原则：

1. 安装前缀固定为当前构建目录下的 `rectify_virtual_camera`
2. 安装规则只围绕打包产物，不干扰现有构建目标
3. 配置文件和脚本通过 `install(FILES ...)` 安装
4. 可执行文件通过 `install(TARGETS ...)` 安装
5. 空目录或兼容目录通过 `install(DIRECTORY ...)` 或等价方式创建

预期使用方式：

```bash
cmake -S . -B build
cmake --build build
cmake --install build --prefix /workspace/VirtualCamera/build/rectify_virtual_camera
```

如果实现中希望减少调用参数，也可以在 `CMakeLists.txt` 中为该安装目录提供明确提示，但不额外引入新的构建命令入口。

## 测试设计

本次新增测试只覆盖打包结构，不覆盖业务算法。

### 测试目标

验证以下内容：

1. 安装后存在 `build/rectify_virtual_camera/virtual_camera_tool`
2. 安装后存在 `build/rectify_virtual_camera/config.json`
3. 安装后存在 `build/rectify_virtual_camera/image_virtual.bash`
4. 安装后存在 `build/rectify_virtual_camera/output/x86/lib`
5. 包内 `config.json` 内容来自并行配置文件
6. `image_virtual.bash` 不包含 `build/` 路径引用
7. `image_virtual.bash` 默认调用 `./virtual_camera_tool ./config.json`

### 测试形式

优先采用现有测试体系外的轻量打包验证脚本，原因如下：

- 当前已有单元测试主要覆盖 C++ 逻辑
- 打包结果更适合通过 shell 级检查验证
- 不需要为了安装路径校验再引入额外 C++ 测试目标

建议新增一个脚本级测试或验证脚本，执行以下动作：

1. 触发构建
2. 执行安装到临时或固定打包目录
3. 校验目录结构
4. 比较根目录 `config.json` 与源并行配置文件是否一致
5. 检查脚本文本内容是否符合预期

如果当前仓库要求所有测试都通过 CMake 暴露，也可以把该校验作为单独的 shell 测试入口挂到现有脚本体系，但不需要把它塞进业务二进制单元测试。

## 文件改动范围

预计涉及以下文件：

- `CMakeLists.txt`
  - 新增安装规则
- `image_virtual.bash`
  - 新增包启动脚本
- `README.md`
  - 补充打包与下游运行说明
- `scripts/` 下新增或修改一个打包验证脚本
  - 用于验证安装结果是否符合预期

## 风险与处理

### 风险 1：运行时仍存在未识别的动态库依赖

虽然当前项目主要链接 vendored OpenCV 静态库和系统库，但 `yaml-cpp` 可能仍以动态库形式参与运行。

处理方式：

- 在实现阶段通过 `ldd` 检查 `virtual_camera_tool`
- 如果发现必须随包提供的非系统 `.so`，统一安装到 `output/x86/lib`
- 如需该目录参与运行时查找，再评估是否在脚本中补充相对路径 `LD_LIBRARY_PATH`

本次设计不预先假设一定需要此逻辑，但保留目录位以便补齐。

### 风险 2：安装目录与现有 `build/` 构建目录相互覆盖

处理方式：

- 安装目标明确固定为 `build/rectify_virtual_camera`
- 不把包内容回写到普通构建产物根目录

### 风险 3：脚本被从非包根目录调用时相对路径失效

处理方式：

- `image_virtual.bash` 启动后先切换到脚本自身目录，再执行包内命令

## 验收标准

满足以下条件即视为完成：

1. 构建后可生成 `build/rectify_virtual_camera`
2. 包目录包含：
   - `virtual_camera_tool`
   - `config.json`
   - `image_virtual.bash`
   - `output/x86/lib`
3. `config.json` 的内容与 `configs/config_rt024_parallel_run.json` 一致
4. `image_virtual.bash` 中不包含 `build/` 路径
5. 下游进入 `build/rectify_virtual_camera` 后可直接执行：

```bash
bash image_virtual.bash
```

6. 不修改现有核心算法与编译选项
