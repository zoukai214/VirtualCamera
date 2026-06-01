# RT024 Runtime CLI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 RT024 的运行时根路径从配置文件移到命令行，支持 `--dataset_root`、`--config_path`、可选 `--output_root` 和显式 `--debug --golden_root`，并让默认运行不做真值对比。

**Architecture:** 先把命令行解析和运行时参数收口到一个小模块，再把 `PipelineConfig` 退回只负责静态任务配置，最后在 `main.cpp` 里把“加载配置、注入根路径、运行 pipeline、按 debug 决定是否校验”串起来。脚本只做参数检查和透传，README 和样例配置同步改成新的运行方式。

**Tech Stack:** C++17、nlohmann::json、std::thread、bash、现有轻量测试程序、CMake/catkin

---

## 文件结构

### 将修改

- `include/virtual_camera/pipeline_config.h`
  - 移除 `golden_root`，保留 `dataset_root` 和 `output_root` 作为运行时注入字段。
- `src/pipeline_config.cpp`
  - 不再从 JSON 读取 `dataset_root`、`output_root`、`golden_root`。
- `src/main.cpp`
  - 改为解析命令行参数，按 debug 决定是否执行真值对比。
- `image_virtual.bash`
  - 改成显式参数脚本，校验 `--dataset_root`、`--config_path`、`--output_root`、`--debug`、`--golden_root`。
- `configs/config_rt024.json`
  - 移除三个运行时根路径字段。
- `configs/config_rt024_parallel_run.json`
  - 移除三个运行时根路径字段。
- `README.md`
  - 更新运行命令、默认行为和 debug 说明。
- `CMakeLists.txt`
  - 注册新的测试目标，并把新的 runtime 源文件加入 `virtual_camera_core`。

### 将新增

- `include/virtual_camera/runtime_args.h`
  - 定义 RT024 运行时参数、usage、解析和运行时注入/验证辅助函数。
- `src/runtime_args.cpp`
  - 实现命令行解析、默认 `output_root` 逻辑、debug 校验和 `MaybeVerifyRt024Outputs()`。
- `tests/test_runtime_args.cpp`
  - 覆盖 CLI 解析、默认值和非法组合。
- `tests/test_rt024_runtime.cpp`
  - 覆盖运行时根路径注入和 debug 门控验证。

---

### Task 1: 新增运行时参数解析

**Files:**
- Create: `include/virtual_camera/runtime_args.h`
- Create: `src/runtime_args.cpp`
- Add Test: `tests/test_runtime_args.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: 先写命令行解析测试**

新增 `tests/test_runtime_args.cpp`，用一组明确的 argv 数组覆盖必填参数、默认值和非法组合：

```cpp
#include "virtual_camera/runtime_args.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void Expect(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void TestParseRt024RuntimeArgsReadsRequiredFields() {
  const char* argv[] = {
      "tool", "--dataset_root", "/data", "--config_path", "configs/config_rt024.json",
      "--output_root", "build/out"};
  const vc::Rt024RuntimeArgs args = vc::ParseRt024RuntimeArgs(7, argv);

  Expect(args.dataset_root == "/data", "dataset_root");
  Expect(args.config_path == "configs/config_rt024.json", "config_path");
  Expect(args.output_root == "build/out", "output_root");
  Expect(!args.debug, "debug");
  Expect(args.golden_root.empty(), "golden_root");
}

void TestParseRt024RuntimeArgsDefaultsOutputRoot() {
  const char* argv[] = {"tool", "--dataset_root", "/data",
                        "--config_path", "configs/config_rt024.json"};
  const vc::Rt024RuntimeArgs args = vc::ParseRt024RuntimeArgs(5, argv);

  Expect(args.output_root == "/data", "default output_root should match dataset_root");
}

void TestParseRt024RuntimeArgsRequiresGoldenRootWhenDebugOn() {
  const char* argv[] = {"tool", "--dataset_root", "/data",
                        "--config_path", "configs/config_rt024.json",
                        "--debug"};
  bool thrown = false;
  try {
    static_cast<void>(vc::ParseRt024RuntimeArgs(6, argv));
  } catch (const vc::UsageError& error) {
    thrown = std::string(error.what()).find("--golden_root") != std::string::npos;
  }
  Expect(thrown, "debug without golden_root should fail");
}

void TestParseRt024RuntimeArgsRejectsGoldenRootWithoutDebug() {
  const char* argv[] = {"tool", "--dataset_root", "/data",
                        "--config_path", "configs/config_rt024.json",
                        "--golden_root", "/golden"};
  bool thrown = false;
  try {
    static_cast<void>(vc::ParseRt024RuntimeArgs(7, argv));
  } catch (const vc::UsageError& error) {
    thrown = std::string(error.what()).find("--debug") != std::string::npos;
  }
  Expect(thrown, "golden_root without debug should fail");
}

}  // namespace

int main() {
  TestParseRt024RuntimeArgsReadsRequiredFields();
  TestParseRt024RuntimeArgsDefaultsOutputRoot();
  TestParseRt024RuntimeArgsRequiresGoldenRootWhenDebugOn();
  TestParseRt024RuntimeArgsRejectsGoldenRootWithoutDebug();
  return 0;
}
```

在 `CMakeLists.txt` 里注册测试目标：

```cmake
foreach(source_file
    src/json_utils.cpp
    src/pipeline_config.cpp
    src/calibration_loader.cpp
    src/undistort_processor.cpp
    src/virtual_camera_processor.cpp
    src/task_builder.cpp
    src/map_generator.cpp
    src/json_writer.cpp
    src/verifier.cpp
    src/runtime_args.cpp
)
```

```cmake
if(EXISTS ${PROJECT_SOURCE_DIR}/tests/test_runtime_args.cpp)
    add_executable(test_runtime_args tests/test_runtime_args.cpp)
    target_link_libraries(test_runtime_args PRIVATE virtual_camera_core)
endif()
```

- [ ] **Step 2: 先跑测试确认当前失败**

Run:

```bash
cmake --build build --target test_runtime_args
./build/test_runtime_args
```

Expected:

```text
编译失败，原因是 `vc::Rt024RuntimeArgs`、`vc::UsageError` 和 `vc::ParseRt024RuntimeArgs` 还未实现
```

- [ ] **Step 3: 实现最小可用解析器**

在 `include/virtual_camera/runtime_args.h` 中定义运行时参数和校验异常：

```cpp
#pragma once

#include <stdexcept>
#include <string>

namespace vc {

class UsageError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

struct Rt024RuntimeArgs {
  std::string dataset_root;
  std::string config_path;
  std::string output_root;
  bool debug = false;
  std::string golden_root;
};

std::string BuildRt024Usage(const std::string& program_name);
Rt024RuntimeArgs ParseRt024RuntimeArgs(int argc, const char* const* argv);

}  // namespace vc
```

在 `src/runtime_args.cpp` 中实现解析和默认值逻辑：

```cpp
#include "virtual_camera/runtime_args.h"

#include <sstream>

namespace vc {
namespace {

void RequireValue(const char* flag, int argc, const char* const* argv, int* index,
                  std::string* value) {
  if (*index + 1 >= argc) {
    throw UsageError(std::string("missing value for ") + flag);
  }
  *value = argv[++(*index)];
}

}  // namespace

std::string BuildRt024Usage(const std::string& program_name) {
  std::ostringstream oss;
  oss << "Usage: " << program_name
      << " --dataset_root <path> --config_path <path> [--output_root <path>]"
      << " [--debug --golden_root <path>]";
  return oss.str();
}

Rt024RuntimeArgs ParseRt024RuntimeArgs(int argc, const char* const* argv) {
  Rt024RuntimeArgs args;
  for (int i = 1; i < argc; ++i) {
    const std::string flag = argv[i];
    if (flag == "--dataset_root") {
      RequireValue("--dataset_root", argc, argv, &i, &args.dataset_root);
    } else if (flag == "--config_path") {
      RequireValue("--config_path", argc, argv, &i, &args.config_path);
    } else if (flag == "--output_root") {
      RequireValue("--output_root", argc, argv, &i, &args.output_root);
    } else if (flag == "--debug") {
      args.debug = true;
    } else if (flag == "--golden_root") {
      RequireValue("--golden_root", argc, argv, &i, &args.golden_root);
    } else {
      throw UsageError("unknown argument: " + flag);
    }
  }

  if (args.dataset_root.empty()) {
    throw UsageError("missing required argument: --dataset_root");
  }
  if (args.config_path.empty()) {
    throw UsageError("missing required argument: --config_path");
  }
  if (args.output_root.empty()) {
    args.output_root = args.dataset_root;
  }
  if (args.debug && args.golden_root.empty()) {
    throw UsageError("missing required argument: --golden_root");
  }
  if (!args.debug && !args.golden_root.empty()) {
    throw UsageError("--golden_root requires --debug");
  }
  return args;
}

}  // namespace vc
```

- [ ] **Step 4: 重新跑测试确认通过**

Run:

```bash
cmake --build build --target test_runtime_args
./build/test_runtime_args
```

Expected:

```text
PASS，退出码为 0
```

- [ ] **Step 5: 提交本任务**

```bash
git add include/virtual_camera/runtime_args.h src/runtime_args.cpp tests/test_runtime_args.cpp CMakeLists.txt
git commit -m "feat: add rt024 runtime cli parser"
```

---

### Task 2: 让配置文件只保留静态任务配置

**Files:**
- Modify: `include/virtual_camera/pipeline_config.h`
- Modify: `src/pipeline_config.cpp`
- Modify: `tests/test_pipeline_config.cpp`
- Modify: `configs/config_rt024.json`
- Modify: `configs/config_rt024_parallel_run.json`

- [ ] **Step 1: 先改配置测试，去掉运行时根路径字段**

把 `tests/test_pipeline_config.cpp` 的成功用例 JSON 从：

```json
{
  "dataset_root": "/workspace/GACRT024_1754812994",
  "golden_root": "/workspace/GACRT024_1754812994",
  "output_root": "build/rt024_output",
  "conf_dir_path": "calib_extract/",
  "image_dir_path": "image_raw/",
  "vc_image_dir_path": "image_virtual_camera/",
  "undistort_image_dir_path": "image_undistortion/",
  "vc_conf_dir_path": "calib_virtual_camera/",
  "undistort_conf_dir_path": "calib_undistortion/",
  "vc_gdcbin_dir_path": "vc_gdcbin_dir_path/",
  "task_parallelism": 2,
  "undistort_parallelism": 3,
  "virtual_camera_parallelism": 4,
  "virtual_camera_configs": [],
  "undistort_configs": []
}
```

改成：

```json
{
  "conf_dir_path": "calib_extract/",
  "image_dir_path": "image_raw/",
  "vc_image_dir_path": "image_virtual_camera/",
  "undistort_image_dir_path": "image_undistortion/",
  "vc_conf_dir_path": "calib_virtual_camera/",
  "undistort_conf_dir_path": "calib_undistortion/",
  "vc_gdcbin_dir_path": "vc_gdcbin_dir_path/",
  "task_parallelism": 2,
  "undistort_parallelism": 3,
  "virtual_camera_parallelism": 4,
  "virtual_camera_configs": [],
  "undistort_configs": []
}
```

并把断言改成只验证静态字段：

```cpp
Expect(config.task_parallelism == 2, "task_parallelism");
Expect(config.undistort_parallelism == 3, "undistort_parallelism");
Expect(config.virtual_camera_parallelism == 4, "virtual_camera_parallelism");
Expect(config.dataset_root.empty(), "dataset_root should be injected later");
Expect(config.output_root.empty(), "output_root should be injected later");
```

把默认值测试也改成不再要求 `dataset_root`、`golden_root`、`output_root` 出现在 JSON 中。

- [ ] **Step 2: 先跑测试确认当前失败**

Run:

```bash
cmake --build build --target test_pipeline_config
./build/test_pipeline_config
```

Expected:

```text
编译或运行失败，原因是 `LoadPipelineConfig()` 还在强制读取 `dataset_root` / `output_root` / `golden_root`
```

- [ ] **Step 3: 把配置解析改成只读静态项**

在 `include/virtual_camera/pipeline_config.h` 中删除 `golden_root` 字段，保留运行时注入的 `dataset_root` 和 `output_root`：

```cpp
struct PipelineConfig {
  OutputPathConfig paths;
  int showinfo = 0;
  int showdir = 0;
  int process_virtual_camera = 0;
  int process_undistort = 0;
  int task_parallelism = 1;
  int undistort_parallelism = 1;
  int virtual_camera_parallelism = 1;
  int undistort_image = 0;
  int distort_model = 0;
  int save_virtual_json = 0;
  int save_undistort_json = 0;
  int conf_type = 0;
  std::string dataset_root;
  std::string output_root;
  std::vector<VirtualCameraTaskConfig> virtual_tasks;
  std::vector<UndistortTaskConfig> undistort_tasks;
};
```

在 `src/pipeline_config.cpp` 里删掉这三行：

```cpp
  cfg.dataset_root = Required<std::string>(json, "dataset_root");
  cfg.golden_root = json.value("golden_root", cfg.dataset_root);
  cfg.output_root = json.value("output_root", cfg.dataset_root);
  cfg.paths.dataset_root = cfg.dataset_root;
```

保留对相对目录、开关、并行度和任务数组的解析不变。

把两个样例配置文件开头的三项移除，保留任务内容不动：

```json
{
  "conf_dir_path": "calib_extract/",
  "image_dir_path": "image_raw/",
  "vc_image_dir_path": "image_virtual_camera/",
  "undistort_image_dir_path": "image_undistortion/",
  "vc_conf_dir_path": "calib_virtual_camera/",
  "undistort_conf_dir_path": "calib_undistortion/",
  "vc_gdcbin_dir_path": "vc_gdcbin_dir_path/",
  ...
}
```

- [ ] **Step 4: 重新跑配置测试**

Run:

```bash
cmake --build build --target test_pipeline_config
./build/test_pipeline_config
```

Expected:

```text
PASS，退出码为 0
```

- [ ] **Step 5: 提交本任务**

```bash
git add include/virtual_camera/pipeline_config.h src/pipeline_config.cpp tests/test_pipeline_config.cpp configs/config_rt024.json configs/config_rt024_parallel_run.json
git commit -m "feat: remove runtime roots from rt024 config"
```

---

### Task 3: 把运行时根路径注入和 debug 校验收口到可测试辅助函数

**Files:**
- Modify: `include/virtual_camera/runtime_args.h`
- Modify: `src/runtime_args.cpp`
- Add Test: `tests/test_rt024_runtime.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: 先写运行时注入和 debug 门控测试**

新增 `tests/test_rt024_runtime.cpp`，直接验证两个核心辅助函数：

```cpp
#include "virtual_camera/runtime_args.h"

#include <functional>
#include <stdexcept>
#include <string>

namespace {

void Expect(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void TestApplyRt024RuntimeArgsSetsRoots() {
  vc::Rt024RuntimeArgs args;
  args.dataset_root = "/workspace/data";
  args.output_root = "/workspace/output";

  vc::PipelineConfig config;
  vc::ApplyRt024RuntimeArgs(args, &config);

  Expect(config.dataset_root == "/workspace/data", "dataset_root");
  Expect(config.output_root == "/workspace/output", "output_root");
  Expect(config.paths.dataset_root == "/workspace/data", "paths.dataset_root");
}

void TestApplyRt024RuntimeArgsDefaultsOutputRoot() {
  vc::Rt024RuntimeArgs args;
  args.dataset_root = "/workspace/data";

  vc::PipelineConfig config;
  vc::ApplyRt024RuntimeArgs(args, &config);

  Expect(config.output_root == "/workspace/data", "default output_root");
}

void TestMaybeVerifyRt024OutputsSkipsWhenDebugDisabled() {
  vc::Rt024RuntimeArgs args;
  vc::PipelineConfig config;
  bool called = false;

  const vc::VerifyResult result = vc::MaybeVerifyRt024Outputs(
      args, config,
      [&](const std::string&, const std::string&) {
        called = true;
        return vc::VerifyResult{true, "verification passed"};
      });

  Expect(!called, "verifier should not run");
  Expect(result.ok, "result.ok");
  Expect(result.message == "pipeline completed", "default message");
}

void TestMaybeVerifyRt024OutputsCallsVerifierWhenDebugEnabled() {
  vc::Rt024RuntimeArgs args;
  args.debug = true;
  args.golden_root = "/workspace/golden";

  vc::PipelineConfig config;
  config.output_root = "/workspace/output";

  bool called = false;
  const vc::VerifyResult result = vc::MaybeVerifyRt024Outputs(
      args, config,
      [&](const std::string& golden_root, const std::string& output_root) {
        called = true;
        Expect(golden_root == "/workspace/golden", "golden_root passed to verifier");
        Expect(output_root == "/workspace/output", "output_root passed to verifier");
        return vc::VerifyResult{true, "verification passed"};
      });

  Expect(called, "verifier should run");
  Expect(result.ok, "result.ok");
  Expect(result.message == "verification passed", "verification message");
}

}  // namespace

int main() {
  TestApplyRt024RuntimeArgsSetsRoots();
  TestApplyRt024RuntimeArgsDefaultsOutputRoot();
  TestMaybeVerifyRt024OutputsSkipsWhenDebugDisabled();
  TestMaybeVerifyRt024OutputsCallsVerifierWhenDebugEnabled();
  return 0;
}
```

在 `CMakeLists.txt` 中注册 `test_rt024_runtime`：

```cmake
if(EXISTS ${PROJECT_SOURCE_DIR}/tests/test_rt024_runtime.cpp)
    add_executable(test_rt024_runtime tests/test_rt024_runtime.cpp)
    target_link_libraries(test_rt024_runtime PRIVATE virtual_camera_core)
endif()
```

- [ ] **Step 2: 先跑测试确认当前失败**

Run:

```bash
cmake --build build --target test_rt024_runtime
./build/test_rt024_runtime
```

Expected:

```text
编译失败，原因是 `ApplyRt024RuntimeArgs()`、`MaybeVerifyRt024Outputs()` 还未实现
```

- [ ] **Step 3: 在 runtime_args.cpp 中补齐注入和校验**

先扩展 `include/virtual_camera/runtime_args.h`，给运行时注入和 debug 门控补齐声明：

```cpp
#pragma once

#include "virtual_camera/pipeline_config.h"
#include "virtual_camera/verifier.h"

#include <functional>
#include <stdexcept>
#include <string>

namespace vc {

class UsageError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

struct Rt024RuntimeArgs {
  std::string dataset_root;
  std::string config_path;
  std::string output_root;
  bool debug = false;
  std::string golden_root;
};

std::string BuildRt024Usage(const std::string& program_name);
Rt024RuntimeArgs ParseRt024RuntimeArgs(int argc, const char* const* argv);
void ApplyRt024RuntimeArgs(const Rt024RuntimeArgs& args, PipelineConfig* config);
VerifyResult MaybeVerifyRt024Outputs(
    const Rt024RuntimeArgs& args, const PipelineConfig& config,
    const std::function<VerifyResult(const std::string&, const std::string&)>& verify_fn);

}  // namespace vc
```

再把 `src/runtime_args.cpp` 扩展为同时实现运行时注入和 debug 门控：

```cpp
void ApplyRt024RuntimeArgs(const Rt024RuntimeArgs& args, PipelineConfig* config) {
  if (config == nullptr) {
    throw std::runtime_error("config must not be null");
  }
  config->dataset_root = args.dataset_root;
  config->output_root = args.output_root.empty() ? args.dataset_root : args.output_root;
  config->paths.dataset_root = config->dataset_root;
}

VerifyResult MaybeVerifyRt024Outputs(
    const Rt024RuntimeArgs& args, const PipelineConfig& config,
    const std::function<VerifyResult(const std::string&, const std::string&)>& verify_fn) {
  if (!args.debug) {
    return {true, "pipeline completed"};
  }
  return verify_fn(args.golden_root, config.output_root);
}
```

同时保留 `ParseRt024RuntimeArgs()` 对非法参数组合的校验不变。

- [ ] **Step 4: 重新跑运行时测试**

Run:

```bash
cmake --build build --target test_runtime_args test_rt024_runtime
./build/test_runtime_args
./build/test_rt024_runtime
```

Expected:

```text
两个测试都 PASS，退出码为 0
```

- [ ] **Step 5: 提交本任务**

```bash
git add include/virtual_camera/runtime_args.h src/runtime_args.cpp tests/test_rt024_runtime.cpp CMakeLists.txt
git commit -m "feat: add rt024 runtime verification helpers"
```

---

### Task 4: 改造入口程序和脚本

**Files:**
- Modify: `src/main.cpp`
- Modify: `image_virtual.bash`
- Modify: `README.md`

- [ ] **Step 1: 先验证旧入口还没支持新参数**

在仓库根目录尝试运行新式参数，确认当前入口仍然只接受旧的单参数方式：

```bash
./build/virtual_camera_tool --dataset_root /workspace/GACRT024_1754812994 --config_path configs/config_rt024_parallel_run.json
```

Expected:

```text
Usage: ./build/virtual_camera_tool <config.json>
```

- [ ] **Step 2: 把 main.cpp 改成新入口**

把 `src/main.cpp` 改成下面的流程：

```cpp
#include "virtual_camera/runtime_args.h"
#include "virtual_camera/pipeline_config.h"
#include "virtual_camera/pipeline_runner.h"
#include "virtual_camera/undistort_processor.h"
#include "virtual_camera/verifier.h"
#include "virtual_camera/virtual_camera_processor.h"

#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
  try {
    const vc::Rt024RuntimeArgs args = vc::ParseRt024RuntimeArgs(argc, argv);
    vc::PipelineConfig config = vc::LoadPipelineConfig(args.config_path);
    vc::ApplyRt024RuntimeArgs(args, &config);

    vc::RunTopLevelPipelines(
        config,
        [&config]() { vc::RunUndistortPipeline(config); },
        [&config]() { vc::RunVirtualCameraPipeline(config); });

    const vc::VerifyResult result = vc::MaybeVerifyRt024Outputs(
        args, config, [](const std::string& golden_root, const std::string& output_root) {
          return vc::VerifyRt024Outputs(golden_root, output_root);
        });
    if (!result.ok) {
      std::cerr << result.message << "\n";
      return 2;
    }
    std::cout << result.message << "\n";
    return 0;
  } catch (const vc::UsageError& error) {
    std::cerr << error.what() << "\n";
    std::cerr << vc::BuildRt024Usage(argv[0]) << "\n";
    return 1;
  } catch (const std::exception& error) {
    std::cerr << error.what() << "\n";
    return 1;
  }
}
```

这个改法要保证：

- 默认不做真值对比。
- `--output_root` 为空时自动回退到 `dataset_root`。
- `--debug` 只有在显式开启时才会触发 `VerifyRt024Outputs()`。
- `UsageError` 打印参数错误和 usage，普通运行时异常不打印 usage。

- [ ] **Step 3: 把脚本改成显式参数包装器**

把 `image_virtual.bash` 改成不依赖当前工作目录的参数包装器：

```bash
#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: image_virtual.bash --dataset_root <path> --config_path <path> [--output_root <path>] [--debug --golden_root <path>]
EOF
}

dataset_root=""
config_path=""
output_root=""
debug=0
golden_root=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --dataset_root)
      [[ $# -ge 2 ]] || { echo "missing value for --dataset_root" >&2; usage >&2; exit 1; }
      dataset_root="$2"
      shift 2
      ;;
    --config_path)
      [[ $# -ge 2 ]] || { echo "missing value for --config_path" >&2; usage >&2; exit 1; }
      config_path="$2"
      shift 2
      ;;
    --output_root)
      [[ $# -ge 2 ]] || { echo "missing value for --output_root" >&2; usage >&2; exit 1; }
      output_root="$2"
      shift 2
      ;;
    --debug)
      debug=1
      shift
      ;;
    --golden_root)
      [[ $# -ge 2 ]] || { echo "missing value for --golden_root" >&2; usage >&2; exit 1; }
      golden_root="$2"
      shift 2
      ;;
    *)
      echo "unknown argument: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

[[ -n "$dataset_root" ]] || { echo "missing required argument: --dataset_root" >&2; usage >&2; exit 1; }
[[ -n "$config_path" ]] || { echo "missing required argument: --config_path" >&2; usage >&2; exit 1; }

if [[ -z "$output_root" ]]; then
  output_root="$dataset_root"
fi

if [[ "$debug" -eq 1 && -z "$golden_root" ]]; then
  echo "missing required argument: --golden_root" >&2
  usage >&2
  exit 1
fi

if [[ "$debug" -eq 0 && -n "$golden_root" ]]; then
  echo "--golden_root requires --debug" >&2
  usage >&2
  exit 1
fi

script_dir="$(cd "$(dirname "$0")" && pwd)"
tool_path="${script_dir}/virtual_camera_tool"

args=(--dataset_root "$dataset_root" --config_path "$config_path" --output_root "$output_root")
if [[ "$debug" -eq 1 ]]; then
  args+=(--debug --golden_root "$golden_root")
fi

"$tool_path" "${args[@]}"
```

注意这里不要 `cd` 到脚本目录，否则调用者传入的相对路径会被脚本环境改写。

- [ ] **Step 4: 更新 README 的运行示例**

把 README 中旧的调用方式：

```bash
./build/virtual_camera_tool configs/config_rt024_parallel_run.json
```

改成新方式，例如：

```bash
./build/virtual_camera_tool \
  --dataset_root /workspace/GACRT024_1754812994 \
  --config_path configs/config_rt024_parallel_run.json \
  --output_root build/rt024_output_parallel_run
```

并补充 debug 运行示例：

```bash
./build/virtual_camera_tool \
  --dataset_root /workspace/GACRT024_1754812994 \
  --config_path configs/config_rt024_parallel_run.json \
  --output_root build/rt024_output_parallel_run \
  --debug \
  --golden_root /workspace/GACRT024_1754812994
```

同时更新“校验与测试”段落，明确：

- 默认模式只执行生成，不做真值对比。
- `--debug` 才会执行 `verification passed` 这类校验输出。
- `--output_root` 未传时默认等于 `--dataset_root`。

- [ ] **Step 5: 先跑脚本参数错误回归**

先准备打包目录，再验证脚本会在参数不完整时直接失败：

```bash
cmake --install build --prefix /workspace/VirtualCamera/build/rectify_virtual_camera
cd /workspace/VirtualCamera/build/rectify_virtual_camera
./image_virtual.bash
./image_virtual.bash --dataset_root /workspace/GACRT024_1754812994
./image_virtual.bash --dataset_root /workspace/GACRT024_1754812994 --config_path ./config.json --debug
./image_virtual.bash --dataset_root /workspace/GACRT024_1754812994 --config_path ./config.json --golden_root /workspace/GACRT024_1754812994
```

Expected:

```text
分别报缺少 --dataset_root、缺少 --config_path、缺少 --golden_root、以及 --golden_root requires --debug
```

- [ ] **Step 6: 提交本任务**

```bash
git add src/main.cpp image_virtual.bash README.md
git commit -m "feat: switch rt024 to runtime cli"
```

---

### Task 5: 完整回归并整理交付说明

**Files:**
- Modify: `README.md`
- Modify: `configs/config_rt024.json`
- Modify: `configs/config_rt024_parallel_run.json`

- [ ] **Step 1: 跑配置和运行时单测**

Run:

```bash
cmake --build build --target test_runtime_args test_rt024_runtime test_pipeline_config test_parallel_executor test_jobs test_rt024_verifier virtual_camera_tool
./build/test_runtime_args
./build/test_rt024_runtime
./build/test_pipeline_config
./build/test_parallel_executor
./build/test_jobs
./build/test_rt024_verifier
```

Expected:

```text
全部 PASS，退出码为 0
```

- [ ] **Step 2: 跑默认模式和 debug 模式的端到端命令**

默认模式：

```bash
./build/virtual_camera_tool \
  --dataset_root /workspace/GACRT024_1754812994 \
  --config_path configs/config_rt024_parallel_run.json \
  --output_root build/rt024_output_parallel_run
```

Expected:

```text
打印 `pipeline completed`
```

debug 模式：

```bash
./build/virtual_camera_tool \
  --dataset_root /workspace/GACRT024_1754812994 \
  --config_path configs/config_rt024_parallel_run.json \
  --output_root build/rt024_output_parallel_run \
  --debug \
  --golden_root /workspace/GACRT024_1754812994
```

Expected:

```text
打印 `verification passed`
```

- [ ] **Step 3: 更新 README 中的验证命令列表**

把现有旧命令：

```bash
./build/virtual_camera_tool configs/config_rt024_parallel_run.json
```

改成新的命令列表，并把“输出目录”一节改成说明 `output_root` 默认等于 `dataset_root`，只有显式传参时才写到别的地方。

- [ ] **Step 4: 做最终一致性检查**

Run:

```bash
grep -RInE '"dataset_root"|\"golden_root\"|\"output_root\"' configs
grep -RInE -- '--dataset_root|--config_path|--output_root|--debug|--golden_root' README.md image_virtual.bash src tests include
```

Expected:

```text
第一个 grep 没有输出，说明配置文件中已移除三个运行时根路径字段；第二个 grep 能看到脚本、README 和运行时解析代码里的新参数语义
```

- [ ] **Step 5: 提交最终整理**

```bash
git add README.md configs/config_rt024.json configs/config_rt024_parallel_run.json
git commit -m "docs: refresh rt024 runtime cli usage"
```

---

## Self-Review Checklist

- `dataset_root`、`config_path`、`output_root`、`debug`、`golden_root` 都有明确的解析和校验任务。
- 配置文件移除三个运行时根路径字段有专门任务覆盖。
- 默认不执行真值对比，`--debug` 才执行真值对比有单测和端到端命令覆盖。
- 脚本参数检查有明确的错误场景和预期输出。
- 每个会改代码的任务都给了具体文件、具体代码和可执行命令，没有占位词。
