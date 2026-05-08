# Thor Config Include Loading Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add compatible Thor config loading so existing full JSON files still work and new include-based master configs can merge multiple JSON files.

**Architecture:** Add a focused `LoadThorConfig(config_path)` API beside the existing JSON helpers. The loader returns the original JSON when no `include` key exists, or a merged JSON object when `include` lists child files relative to the master config directory. Existing task generation remains centered on `BuildThorTasks`, so map generation and verification do not need to know whether config came from one file or several.

**Tech Stack:** C++17, nlohmann_json, `<filesystem>`, existing shell build via `./scripts/build.sh`.

---

## File Structure

- Modify: `include/virtual_camera/task_builder.h`
  - Declare `nlohmann::json LoadThorConfig(const std::string& config_path);`.
- Modify: `src/task_builder.cpp`
  - Implement include-aware loading and JSON merge helpers in the anonymous namespace.
  - Keep `BuildThorTasks` behavior unchanged.
- Modify: `src/main.cpp`
  - Replace direct `ReadJson(config_path)` with `LoadThorConfig(config_path)`.
- Modify: `tests/test_task_builder.cpp`
  - Keep existing task count checks for full config.
  - Add include-format loading tests using temporary JSON files.
  - Add invalid include type test.

## Task 1: Add Failing Tests For Include Loading

**Files:**
- Modify: `tests/test_task_builder.cpp`

- [ ] **Step 1: Replace `tests/test_task_builder.cpp` with failing tests**

```cpp
#include "virtual_camera/json_utils.h"
#include "virtual_camera/task_builder.h"

#include <filesystem>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>

namespace {

bool CheckThorTaskCounts(const std::vector<vc::CameraTask>& tasks,
                         const std::string& context) {
  int virtual_count = 0;
  int undistort_count = 0;
  int resize_count = 0;
  std::set<std::string> bin_names;

  for (const auto& task : tasks) {
    bin_names.insert(task.bin_name);
    if (task.type == vc::TaskType::kVirtual) {
      ++virtual_count;
    } else if (task.type == vc::TaskType::kUndistort) {
      ++undistort_count;
    } else if (task.type == vc::TaskType::kResize) {
      ++resize_count;
    }
  }

  if (virtual_count != 9) {
    std::cerr << context << ": expected 9 virtual tasks, got " << virtual_count << "\n";
    return false;
  }
  if (undistort_count != 7) {
    std::cerr << context << ": expected 7 undistort tasks, got " << undistort_count << "\n";
    return false;
  }
  if (resize_count != 3) {
    std::cerr << context << ": expected 3 resize tasks, got " << resize_count << "\n";
    return false;
  }
  if (!bin_names.count("gdc_ft30.bin") || !bin_names.count("gdc_fw120_1080.bin")) {
    std::cerr << context << ": expected Thor bin names are missing\n";
    return false;
  }
  return true;
}

nlohmann::json MakeVirtualCamera(const std::string& conf_json, int camera_id,
                                 const std::string& bin_name,
                                 const std::string& undistort_bin_name) {
  return {
      {"fov", 30},
      {"image_height", 2160},
      {"image_width", 3840},
      {"desc", conf_json},
      {"conf_json", conf_json},
      {"intri_json", conf_json + "/" + conf_json + "-intrinsic.json"},
      {"extri_json", conf_json + "/" + conf_json + "-to-car_center-extrinsic.json"},
      {"camera_id", camera_id},
      {"bin_name", bin_name},
      {"undis_bin_name", undistort_bin_name},
      {"new_intrinsic",
       {{"fov", 30},
        {"focal_u", 1910.8},
        {"focal_v", 1910.8},
        {"center_u", 512.0},
        {"center_v", 256.0},
        {"image_width", 1024},
        {"image_height", 512}}},
      {"new_extrinsics", {{"pitch", 0.0}, {"roll", 0.0}, {"yaw", 0.0}}},
  };
}

bool TestFullConfigStillLoads() {
  const auto config = vc::LoadThorConfig("configs/config_thor.json");
  return CheckThorTaskCounts(vc::BuildThorTasks(config), "full config");
}

bool TestIncludeConfigMergesChildFiles() {
  const std::filesystem::path root = "build/test_tmp/include_config";
  std::filesystem::remove_all(root);
  vc::EnsureDirectory((root / "thor").string());

  vc::WriteJson((root / "thor/common.json").string(),
                {{"showinfo", true}, {"virtual_camera_configs", nlohmann::json::array()}});
  vc::WriteJson((root / "thor/virtual_a.json").string(),
                {{"virtual_camera_configs",
                  nlohmann::json::array(
                      {MakeVirtualCamera("front_a", 0, "gdc_front_a.bin", "ldc_front_a.bin")})}});
  vc::WriteJson((root / "thor/virtual_b.json").string(),
                {{"virtual_camera_configs",
                  nlohmann::json::array(
                      {MakeVirtualCamera("front_b", 1, "gdc_front_b.bin", "ldc_front_b.bin")})}});
  vc::WriteJson((root / "thor/resize.json").string(),
                {{"virtual_init_camera_config",
                  nlohmann::json::array(
                      {MakeVirtualCamera("resize_a", 2, "gdc_resize_a.bin", "")})}});
  vc::WriteJson((root / "config.json").string(),
                {{"include",
                  nlohmann::json::array({"thor/common.json", "thor/virtual_a.json",
                                         "thor/virtual_b.json", "thor/resize.json"})}});

  const auto config = vc::LoadThorConfig((root / "config.json").string());
  const auto tasks = vc::BuildThorTasks(config);

  int virtual_count = 0;
  int undistort_count = 0;
  int resize_count = 0;
  std::set<std::string> bin_names;
  for (const auto& task : tasks) {
    bin_names.insert(task.bin_name);
    if (task.type == vc::TaskType::kVirtual) {
      ++virtual_count;
    } else if (task.type == vc::TaskType::kUndistort) {
      ++undistort_count;
    } else if (task.type == vc::TaskType::kResize) {
      ++resize_count;
    }
  }

  if (virtual_count != 2 || undistort_count != 2 || resize_count != 1) {
    std::cerr << "include config: unexpected task counts: virtual=" << virtual_count
              << ", undistort=" << undistort_count << ", resize=" << resize_count << "\n";
    return false;
  }
  if (!bin_names.count("gdc_front_a.bin") || !bin_names.count("ldc_front_b.bin") ||
      !bin_names.count("gdc_resize_a.bin")) {
    std::cerr << "include config: merged bin names are missing\n";
    return false;
  }
  return true;
}

bool TestInvalidIncludeTypeThrows() {
  const std::filesystem::path root = "build/test_tmp/invalid_include";
  std::filesystem::remove_all(root);
  vc::EnsureDirectory(root.string());
  vc::WriteJson((root / "config.json").string(), {{"include", "thor/common.json"}});

  try {
    (void)vc::LoadThorConfig((root / "config.json").string());
  } catch (const std::runtime_error& ex) {
    const std::string message = ex.what();
    if (message.find("include must be an array") != std::string::npos) {
      return true;
    }
    std::cerr << "invalid include: unexpected error: " << message << "\n";
    return false;
  }

  std::cerr << "invalid include: expected exception\n";
  return false;
}

}  // namespace

int main() {
  if (!TestFullConfigStillLoads()) {
    return 1;
  }
  if (!TestIncludeConfigMergesChildFiles()) {
    return 1;
  }
  if (!TestInvalidIncludeTypeThrows()) {
    return 1;
  }
  return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```bash
./scripts/build.sh
```

Expected: build fails because `vc::LoadThorConfig` is not declared.

- [ ] **Step 3: Commit the failing test**

Do not commit failing tests alone. Continue to Task 2 before committing.

## Task 2: Implement LoadThorConfig

**Files:**
- Modify: `include/virtual_camera/task_builder.h`
- Modify: `src/task_builder.cpp`
- Test: `tests/test_task_builder.cpp`

- [ ] **Step 1: Declare the loader**

In `include/virtual_camera/task_builder.h`, add this declaration before `BuildThorTasks`:

```cpp
nlohmann::json LoadThorConfig(const std::string& config_path);
```

- [ ] **Step 2: Add merge helpers in `src/task_builder.cpp`**

Add these helpers inside the anonymous namespace, before `ParseVirtualParam`:

```cpp
void MergeJsonObject(const nlohmann::json& source, nlohmann::json* destination) {
  if (!source.is_object()) {
    throw std::runtime_error("included config must be a json object");
  }

  for (const auto& item : source.items()) {
    const std::string& key = item.key();
    const nlohmann::json& value = item.value();
    if (destination->contains(key) && (*destination)[key].is_array() && value.is_array()) {
      for (const auto& element : value) {
        (*destination)[key].push_back(element);
      }
    } else {
      (*destination)[key] = value;
    }
  }
}

nlohmann::json LoadIncludedThorConfig(const nlohmann::json& config,
                                      const std::filesystem::path& config_dir) {
  if (!config.at("include").is_array()) {
    throw std::runtime_error("include must be an array");
  }

  nlohmann::json merged = nlohmann::json::object();
  for (const auto& include_item : config.at("include")) {
    if (!include_item.is_string()) {
      throw std::runtime_error("include item must be a string");
    }
    const std::filesystem::path include_path =
        config_dir / include_item.get<std::string>();
    const auto child_config = ReadJson(include_path.string());
    if (child_config.contains("include")) {
      throw std::runtime_error("nested include is not supported: " + include_path.string());
    }
    MergeJsonObject(child_config, &merged);
  }
  return merged;
}
```

- [ ] **Step 3: Define the public loader in `src/task_builder.cpp`**

Add this function before `BuildThorTasks`:

```cpp
nlohmann::json LoadThorConfig(const std::string& config_path) {
  const auto config = ReadJson(config_path);
  if (!config.is_object()) {
    throw std::runtime_error("thor config must be a json object: " + config_path);
  }
  if (!config.contains("include")) {
    return config;
  }

  const std::filesystem::path config_dir =
      std::filesystem::path(config_path).parent_path();
  return LoadIncludedThorConfig(config, config_dir);
}
```

- [ ] **Step 4: Run test to verify it passes**

Run:

```bash
./scripts/build.sh
./build/test_task_builder
```

Expected: both commands pass. `test_task_builder` exits with status 0.

- [ ] **Step 5: Commit loader and tests**

```bash
git add include/virtual_camera/task_builder.h src/task_builder.cpp tests/test_task_builder.cpp
git commit -m "feat: support thor config includes"
```

## Task 3: Use Loader From CLI And Run Verification

**Files:**
- Modify: `src/main.cpp`
- Test: `tests/test_task_builder.cpp`
- Test: `tests/test_verifier.cpp`

- [ ] **Step 1: Switch `src/main.cpp` to include-aware loading**

Change:

```cpp
const auto config = vc::ReadJson(config_path);
```

to:

```cpp
const auto config = vc::LoadThorConfig(config_path);
```

- [ ] **Step 2: Run build and unit tests**

Run:

```bash
./scripts/build.sh
./build/test_task_builder
./build/test_verifier
```

Expected: all commands pass.

- [ ] **Step 3: Run full Thor verification when local data exists**

Run:

```bash
if [ -d /workspace/L022/cfg/7v ]; then ./scripts/run_thor_verify.sh; else echo "skip thor verify: /workspace/L022/cfg/7v not found"; fi
```

Expected: either the Thor verification passes, or the command prints `skip thor verify: /workspace/L022/cfg/7v not found`.

- [ ] **Step 4: Commit CLI integration**

```bash
git add src/main.cpp
git commit -m "fix: load thor config through include-aware loader"
```

## Self-Review

- Spec coverage: The plan preserves old full JSON loading, adds include-based child loading, keeps `BuildThorTasks` as the compatibility boundary, validates include type, rejects nested include, and tests both old and new formats.
- Placeholder scan: No placeholder steps remain; each code step includes exact content and commands.
- Type consistency: `LoadThorConfig(const std::string&)` is declared in `task_builder.h`, defined in `task_builder.cpp`, and called from `src/main.cpp` and tests. JSON merge helpers use `nlohmann::json` consistently.
