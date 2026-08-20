# Remove Rt024 Symbol Names Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove `Rt024` from C++ function, type, and test function names while preserving RT024 dataset/config identifiers.

**Architecture:** This is a mechanical API rename with no behavior changes. Public headers, source definitions, call sites, and test function names are updated together so the build catches any missed references. Runtime data paths such as `/workspace/GACRT024_1754812994`, config filenames such as `config_rt024.json`, and README dataset wording stay unchanged.

**Tech Stack:** C++17, CMake, vendored OpenCV/Eigen/nlohmann_json, yaml-cpp.

---

### Task 1: Rename Public API Symbols

**Files:**
- Modify: `include/virtual_camera/calibration_loader.h`
- Modify: `include/virtual_camera/verifier.h`
- Modify: `include/virtual_camera/logging.h`
- Modify: `include/virtual_camera/runtime_args.h`
- Modify: `include/virtual_camera/json_writer.h`
- Modify: `src/*.cpp`
- Modify: `tests/*.cpp`

- [ ] **Step 1: Rename symbols only**

Replace these identifiers in main project code and tests:

```text
LoadRt024Calibration -> LoadCalibration
VerifyRt024Outputs -> VerifyOutputs
BuildRt024PipelineStartMessage -> BuildPipelineStartMessage
Rt024RuntimeArgs -> RuntimeArgs
BuildRt024Usage -> BuildUsage
ParseRt024RuntimeArgs -> ParseRuntimeArgs
ApplyRt024RuntimeArgs -> ApplyRuntimeArgs
MaybeVerifyRt024Outputs -> MaybeVerifyOutputs
WriteRt024UndistortJson -> WriteUndistortJson
WriteRt024VirtualJson -> WriteVirtualJson
```

- [ ] **Step 2: Keep RT024 data identifiers unchanged**

Do not rename:

```text
/workspace/GACRT024_1754812994
configs/config_rt024.json
configs/config_rt024_parallel_run.json
test_rt024_*.cpp
README dataset descriptions
```

- [ ] **Step 3: Verify no code-symbol residue**

Run:

```bash
grep -RIn --exclude-dir=.git --exclude-dir=.worktrees --exclude-dir=build --exclude-dir=devel --exclude-dir=install --exclude-dir=third_party --exclude-dir=output_verify -E "LoadRt024|VerifyRt024|BuildRt024|Rt024RuntimeArgs|ParseRt024|ApplyRt024|MaybeVerifyRt024|WriteRt024" include src tests
```

Expected: no output.

- [ ] **Step 4: Build and test**

Run:

```bash
cmake -S . -B /tmp/virtual_camera_rt024_rename_build
cmake --build /tmp/virtual_camera_rt024_rename_build -j2
ctest --test-dir /tmp/virtual_camera_rt024_rename_build --output-on-failure
```

Expected: build succeeds and available tests pass.

- [ ] **Step 5: Run RT024 dataset verification**

Run:

```bash
/tmp/virtual_camera_rt024_rename_build/virtual_camera_tool \
  --dataset_root /workspace/GACRT024_1754812994 \
  --config_path configs/config_rt024_parallel_run.json \
  --output_root /tmp/virtual_camera_rt024_rename_output \
  --debug \
  --golden_root /workspace/GACRT024_1754812994
```

Expected: command exits 0 and debug output verification passes.
