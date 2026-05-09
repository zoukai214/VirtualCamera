# Parallel Camera Generation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add controlled `--jobs N` parallel generation for 7v and 4v camera maps while keeping outputs byte-compatible with existing golden data.

**Architecture:** Add small header-only utilities for jobs parsing and bounded parallel loops. Wire jobs through `main.cpp`, `four_view_runner`, and `CameraMapsGenerator`, keeping dependency-sensitive 4v weight phases staged. Tests cover argument parsing, parallel loop behavior, and full 7v/4v golden verification.

**Tech Stack:** C++17, OpenCV, Eigen, nlohmann_json, yaml-cpp, shell verification scripts.

---

### Task 1: Add Jobs Parsing And Parallel Utility Tests

**Files:**
- Create: `include/virtual_camera/jobs.h`
- Test: `tests/test_jobs.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing test**

Create `tests/test_jobs.cpp`:

```cpp
#include "virtual_camera/jobs.h"

#include <atomic>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

bool ExpectEqual(int actual, int expected, const std::string& context) {
  if (actual != expected) {
    std::cerr << context << ": expected " << expected << ", got " << actual << "\n";
    return false;
  }
  return true;
}

bool TestParseJobs() {
  vc::JobsConfig config;
  const char* default_argv[] = {"tool", "generate-4v", "config.yaml"};
  if (!vc::ParseOptionalJobs(3, default_argv, 3, &config)) {
    std::cerr << "default jobs should parse\n";
    return false;
  }
  if (!config.auto_jobs || config.jobs != 0) {
    std::cerr << "default jobs should use auto mode\n";
    return false;
  }

  const char* explicit_argv[] = {"tool", "generate-4v", "config.yaml", "--jobs", "4"};
  if (!vc::ParseOptionalJobs(5, explicit_argv, 3, &config)) {
    std::cerr << "--jobs 4 should parse\n";
    return false;
  }
  if (config.auto_jobs || config.jobs != 4) {
    std::cerr << "--jobs 4 should produce explicit jobs=4\n";
    return false;
  }

  const char* auto_argv[] = {"tool", "generate-4v", "config.yaml", "--jobs", "0"};
  if (!vc::ParseOptionalJobs(5, auto_argv, 3, &config)) {
    std::cerr << "--jobs 0 should parse\n";
    return false;
  }
  if (!config.auto_jobs || config.jobs != 0) {
    std::cerr << "--jobs 0 should use auto mode\n";
    return false;
  }

  const char* bad_argv[] = {"tool", "generate-4v", "config.yaml", "--jobs", "-1"};
  if (vc::ParseOptionalJobs(5, bad_argv, 3, &config)) {
    std::cerr << "--jobs -1 should fail\n";
    return false;
  }

  const char* missing_argv[] = {"tool", "generate-4v", "config.yaml", "--jobs"};
  if (vc::ParseOptionalJobs(4, missing_argv, 3, &config)) {
    std::cerr << "missing jobs value should fail\n";
    return false;
  }

  return true;
}

bool TestResolveJobs() {
  if (!ExpectEqual(vc::ResolveJobs({0, true}, 8), 8, "auto jobs")) {
    return false;
  }
  if (!ExpectEqual(vc::ResolveJobs({0, true}, 0), 1, "auto fallback")) {
    return false;
  }
  if (!ExpectEqual(vc::ResolveJobs({3, false}, 8), 3, "explicit jobs")) {
    return false;
  }
  return true;
}

bool TestParallelForRunsAllItems() {
  std::atomic<int> count{0};
  vc::ParallelFor(16, 4, [&](std::size_t) { ++count; });
  return ExpectEqual(count.load(), 16, "parallel count");
}

bool TestParallelForPropagatesException() {
  try {
    vc::ParallelFor(4, 2, [](std::size_t index) {
      if (index == 2) {
        throw std::runtime_error("boom");
      }
    });
  } catch (const std::runtime_error& ex) {
    const std::string message = ex.what();
    if (message.find("parallel task failed") != std::string::npos &&
        message.find("boom") != std::string::npos) {
      return true;
    }
    std::cerr << "unexpected exception message: " << message << "\n";
    return false;
  }
  std::cerr << "expected exception\n";
  return false;
}

}  // namespace

int main() {
  if (!TestParseJobs()) return 1;
  if (!TestResolveJobs()) return 1;
  if (!TestParallelForRunsAllItems()) return 1;
  if (!TestParallelForPropagatesException()) return 1;
  return 0;
}
```

Add the executable to `CMakeLists.txt`:

```cmake
if(EXISTS ${PROJECT_SOURCE_DIR}/tests/test_jobs.cpp)
    add_executable(test_jobs tests/test_jobs.cpp)
    target_link_libraries(test_jobs PRIVATE virtual_camera_core)
endif()
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```bash
bash scripts/build.sh
```

Expected: compile fails because `virtual_camera/jobs.h` does not exist.

- [ ] **Step 3: Implement minimal jobs utility**

Create `include/virtual_camera/jobs.h` with:

```cpp
#pragma once

#include <atomic>
#include <cstddef>
#include <exception>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace vc {

struct JobsConfig {
  int jobs = 0;
  bool auto_jobs = true;
};

inline bool ParsePositiveInt(const char* text, int* value) {
  if (text == nullptr || *text == '\0') {
    return false;
  }
  int parsed = 0;
  for (const char* cursor = text; *cursor != '\0'; ++cursor) {
    if (*cursor < '0' || *cursor > '9') {
      return false;
    }
    parsed = parsed * 10 + (*cursor - '0');
  }
  *value = parsed;
  return true;
}

inline bool ParseOptionalJobs(int argc, const char* const* argv, int first_optional,
                              JobsConfig* config) {
  config->jobs = 0;
  config->auto_jobs = true;
  if (argc == first_optional) {
    return true;
  }
  if (argc != first_optional + 2 || std::string(argv[first_optional]) != "--jobs") {
    return false;
  }
  int parsed = 0;
  if (!ParsePositiveInt(argv[first_optional + 1], &parsed)) {
    return false;
  }
  config->jobs = parsed;
  config->auto_jobs = parsed == 0;
  return true;
}

inline int ResolveJobs(const JobsConfig& config, unsigned int hardware_jobs) {
  if (!config.auto_jobs) {
    return config.jobs > 0 ? config.jobs : 1;
  }
  return hardware_jobs > 0 ? static_cast<int>(hardware_jobs) : 1;
}

template <typename Fn>
void ParallelFor(std::size_t count, int jobs, Fn fn) {
  if (count == 0) {
    return;
  }
  const std::size_t worker_count =
      std::min<std::size_t>(count, static_cast<std::size_t>(std::max(1, jobs)));
  std::atomic<std::size_t> next{0};
  std::mutex error_mutex;
  std::vector<std::string> errors;
  std::vector<std::thread> workers;
  workers.reserve(worker_count);

  for (std::size_t worker = 0; worker < worker_count; ++worker) {
    workers.emplace_back([&]() {
      while (true) {
        const std::size_t index = next.fetch_add(1);
        if (index >= count) {
          break;
        }
        try {
          fn(index);
        } catch (const std::exception& ex) {
          std::lock_guard<std::mutex> lock(error_mutex);
          errors.push_back(ex.what());
        } catch (...) {
          std::lock_guard<std::mutex> lock(error_mutex);
          errors.push_back("unknown exception");
        }
      }
    });
  }

  for (auto& worker : workers) {
    worker.join();
  }
  if (!errors.empty()) {
    throw std::runtime_error("parallel task failed: " + errors.front());
  }
}

}  // namespace vc
```

- [ ] **Step 4: Run test to verify it passes**

Run:

```bash
bash scripts/build.sh
./build/test_jobs
```

Expected: both commands pass.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt include/virtual_camera/jobs.h tests/test_jobs.cpp
git commit -m "feat: add jobs utility"
```

### Task 2: Wire `--jobs` Through CLI And 7v Generation

**Files:**
- Modify: `src/main.cpp`
- Test: `tests/test_jobs.cpp`

- [ ] **Step 1: Extend failing CLI parse test**

Add this case to `TestParseJobs()` in `tests/test_jobs.cpp`:

```cpp
const char* verify_argv[] = {
    "tool", "generate-verify", "input", "config.json", "output", "--jobs", "2"};
if (!vc::ParseOptionalJobs(7, verify_argv, 5, &config)) {
  std::cerr << "generate-verify --jobs 2 should parse\n";
  return false;
}
if (config.auto_jobs || config.jobs != 2) {
  std::cerr << "generate-verify --jobs 2 should produce explicit jobs=2\n";
  return false;
}
```

- [ ] **Step 2: Run test to verify it passes but CLI still lacks support**

Run:

```bash
bash scripts/build.sh
./build/test_jobs
./build/virtual_camera_tool generate-verify /workspace/L022/cfg/7v configs/config_thor.json output_verify/7v --jobs 1
```

Expected: `test_jobs` passes; `virtual_camera_tool` prints usage because CLI has not been wired yet.

- [ ] **Step 3: Implement CLI jobs support and parallel 7v tasks**

Modify `src/main.cpp`:

- Include `virtual_camera/jobs.h`.
- Change `GenerateVerify()` to accept `int jobs`.
- Extract the per-task body into a lambda passed to `vc::ParallelFor(tasks.size(), jobs, ...)`.
- Parse optional jobs for both commands.
- Pass resolved jobs into `GenerateVerify()` and `RunFourViewGenerate()`.

- [ ] **Step 4: Run test to verify 7v serial jobs path passes**

Run:

```bash
bash scripts/build.sh
./build/test_jobs
rm -rf output_verify/7v
./build/virtual_camera_tool generate-verify /workspace/L022/cfg/7v configs/config_thor.json output_verify/7v --jobs 1
```

Expected: build and unit test pass; 7v verification prints success.

- [ ] **Step 5: Commit**

```bash
git add src/main.cpp tests/test_jobs.cpp
git commit -m "feat: parallelize 7v generation"
```

### Task 3: Parallelize 4v Camera Map Generation

**Files:**
- Modify: `include/virtual_camera/four_view_runner.h`
- Modify: `src/four_view_runner.cpp`
- Modify: `third_party/gdc_add_cylinder/include/camera_maps_generator.h`
- Modify: `third_party/gdc_add_cylinder/src/camera_maps_generator.cpp`

- [ ] **Step 1: Run current 4v verification to capture baseline**

Run:

```bash
bash scripts/build.sh
bash scripts/run_4v_verify.sh
```

Expected: this may fail on `*weight.bin`; record the first failing file so the later fix can be verified.

- [ ] **Step 2: Wire jobs signatures**

Change:

```cpp
int RunFourViewGenerate(const std::string& config_path);
```

to:

```cpp
int RunFourViewGenerate(const std::string& config_path, int jobs);
```

Change:

```cpp
static CameraMaps generateCameraMapsOptimized(...);
```

to include:

```cpp
int jobs
```

as the final parameter.

- [ ] **Step 3: Parallelize independent 4v phases**

In `camera_maps_generator.cpp`:

- Include `virtual_camera/jobs.h`.
- Use `vc::ParallelFor(camera_configs.size(), jobs, ...)` for coordinate mapping.
- Keep front/rear weight calculation serial.
- Use `vc::ParallelFor()` for left/right weight calculation after front/rear finishes.
- Keep total weight accumulation serial.
- Use `vc::ParallelFor()` for per-camera normalization writeback.

In `four_view_runner.cpp`:

- Pass `jobs` to `generateCameraMapsOptimized()`.
- Include `virtual_camera/jobs.h`.
- Use `vc::ParallelFor(4, jobs, ...)` inside `GenerateCylinderMaps()` for cylinder output.

- [ ] **Step 4: Run 4v serial and parallel checks**

Run:

```bash
bash scripts/build.sh
rm -rf output_verify/4v
./build/virtual_camera_tool generate-4v configs/config_4v.yaml --jobs 1
bash scripts/run_4v_verify.sh
rm -rf output_verify/4v
./build/virtual_camera_tool generate-4v configs/config_4v.yaml --jobs 4
bash scripts/run_4v_verify.sh
```

Expected: all map, mask, metadata, and weight checks pass against `/workspace/L022/cfg/4v/maps`.

- [ ] **Step 5: Commit**

```bash
git add include/virtual_camera/four_view_runner.h src/four_view_runner.cpp third_party/gdc_add_cylinder/include/camera_maps_generator.h third_party/gdc_add_cylinder/src/camera_maps_generator.cpp
git commit -m "feat: parallelize 4v map generation"
```

### Task 4: Full Regression And Script Compatibility

**Files:**
- Modify if needed: `scripts/run_thor_verify.sh`
- Modify if needed: `scripts/run_4v_verify.sh`

- [ ] **Step 1: Run default script compatibility**

Run:

```bash
bash scripts/run_thor_verify.sh
bash scripts/run_4v_verify.sh
```

Expected: both scripts pass without adding `--jobs`.

- [ ] **Step 2: Run explicit serial and parallel 7v checks**

Run:

```bash
rm -rf output_verify/7v
./build/virtual_camera_tool generate-verify /workspace/L022/cfg/7v configs/config_thor.json output_verify/7v --jobs 1
rm -rf output_verify/7v
./build/virtual_camera_tool generate-verify /workspace/L022/cfg/7v configs/config_thor.json output_verify/7v --jobs 4
```

Expected: both runs pass verification against `/workspace/L022/cfg/7v/calib/gdc`, `gdc_intri`, and `virtual`.

- [ ] **Step 3: Run explicit serial and parallel 4v checks**

Run:

```bash
rm -rf output_verify/4v
./build/virtual_camera_tool generate-4v configs/config_4v.yaml --jobs 1
bash scripts/run_4v_verify.sh
rm -rf output_verify/4v
./build/virtual_camera_tool generate-4v configs/config_4v.yaml --jobs 4
bash scripts/run_4v_verify.sh
```

Expected: both runs pass every checked file, including all `front_weight.bin`, `rear_weight.bin`, `left_weight.bin`, and `right_weight.bin` validations.

- [ ] **Step 4: Commit final script/test adjustments if any**

```bash
git add scripts/run_thor_verify.sh scripts/run_4v_verify.sh
git commit -m "test: verify parallel camera generation"
```

Skip this commit if scripts do not change.
