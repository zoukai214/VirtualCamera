# Pipeline Orchestrator Config-Path Init Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `PipelineOrchestrator` parse `config_path` during construction, keep `dataset_root` in `main.cpp` traversal code, and pass `output_root` into save methods.

**Architecture:** `PipelineConfig` stops carrying runtime roots. `PipelineOrchestrator` owns the parsed config plus the prepared undistort and virtual-camera caches, and exposes source-camera groupings plus per-frame/save methods. `main.cpp` keeps the outer traversal over `args.dataset_root`, while save methods accept `args.output_root` so output placement stays explicit and unchanged.

**Tech Stack:** C++17, OpenCV, Eigen, CMake, existing test executables.

---

### Task 1: Add constructor and API tests

**Files:**
- Modify: `tests/test_pipeline_orchestrator.cpp`
- Modify: `include/virtual_camera/pipeline_orchestrator.h`

- [ ] **Step 1: Add constructor-focused tests**

Add a test that constructs the orchestrator from `config_path` only and checks that `UndistortSourceInputs()` and `VirtualSourceInputs()` are populated from the parsed config.

```cpp
const vc::PipelineOrchestrator orchestrator(config_path_string);
Expect(orchestrator.UndistortSourceInputs().size() == 1,
       "undistort source inputs should be prepared from config path");
Expect(orchestrator.VirtualSourceInputs().size() == 1,
       "virtual source inputs should be prepared from config path");
```

- [ ] **Step 2: Add output-root save tests**

Add tests that call:

```cpp
orchestrator.SaveUndistortArtifacts(output_root_string);
orchestrator.SaveVirtualCameraArtifacts(output_root_string);
orchestrator.ProcessUndistortFrame(camera_id, image, output_root_string, filename);
orchestrator.ProcessVirtualCameraFrame(camera_id, image, output_root_string, filename);
```

and assert the generated files land under `output_root_string`.

- [ ] **Step 3: Run the new test target and verify RED**

Run:

```bash
cmake --build /tmp/virtual_camera_orchestrator_build --target test_pipeline_orchestrator -j2
```

Expected before implementation: compile or link failure because the new constructor and output-root signatures do not exist yet.

### Task 2: Refactor `PipelineOrchestrator`

**Files:**
- Modify: `include/virtual_camera/pipeline_orchestrator.h`
- Modify: `src/pipeline_orchestrator.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Change constructor input**

Replace the existing constructor with:

```cpp
explicit PipelineOrchestrator(const std::string& config_path);
```

Inside the constructor, parse the config with `LoadPipelineConfig(config_path)` and build caches immediately.

- [ ] **Step 2: Remove runtime roots from the class state**

Keep `PipelineConfig` for config-only fields, but stop storing `dataset_root` and `output_root` as class members. The constructor should not depend on runtime roots.

- [ ] **Step 3: Pass output root into save methods**

Update the member functions to:

```cpp
void SaveUndistortArtifacts(const std::string& output_root) const;
void SaveVirtualCameraArtifacts(const std::string& output_root) const;
void ProcessUndistortFrame(int camera_id, const cv::Mat& image,
                           const std::string& output_root,
                           const std::string& input_filename) const;
void ProcessVirtualCameraFrame(int camera_id, const cv::Mat& image,
                               const std::string& output_root,
                               const std::string& input_filename) const;
```

- [ ] **Step 4: Keep source-camera grouping available**

Keep `UndistortSourceInputs()` and `VirtualSourceInputs()` as config-derived camera groupings so `main.cpp` can keep its outer traversal.

- [ ] **Step 5: Register new source file**

Add `src/pipeline_orchestrator.cpp` to `virtual_camera_core` in `CMakeLists.txt` and keep the test target wired up.

### Task 3: Update `main.cpp` to use config_path constructor

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Construct orchestrator from config path**

Replace `PipelineOrchestrator orchestrator(config)` with:

```cpp
const vc::PipelineOrchestrator orchestrator(args.config_path);
```

- [ ] **Step 2: Keep `args.dataset_root` traversal in `main.cpp`**

Use `args.dataset_root` to build input directories for both pipelines.

- [ ] **Step 3: Pass `args.output_root` into save/process calls**

Update all save and frame-processing calls to pass `args.output_root` explicitly.

### Task 4: Verify

**Files:**
- No source changes.

- [ ] **Step 1: Build**

Run:

```bash
cmake -S . -B /tmp/virtual_camera_orchestrator_build
cmake --build /tmp/virtual_camera_orchestrator_build -j2
```

- [ ] **Step 2: Run tests**

Run all executables in `/tmp/virtual_camera_orchestrator_build/test_*`.

- [ ] **Step 3: Run dataset regression**

Run:

```bash
/tmp/virtual_camera_orchestrator_build/virtual_camera_tool \
  --dataset_root /workspace/GACRT024_1754812994 \
  --config_path configs/config_rt024_parallel_run.json \
  --output_root /tmp/virtual_camera_orchestrator_output \
  --debug \
  --golden_root /workspace/GACRT024_1754812994
```

Expected: `verification passed`.
