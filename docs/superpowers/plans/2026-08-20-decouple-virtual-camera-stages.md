# Decouple Virtual Camera Stages Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Split virtual camera processing into cache generation, artifact saving, and per-frame remap APIs while keeping existing config switches and output layout unchanged.

**Architecture:** `RunTopLevelPipelines()` keeps using `process_undistort` and `process_virtual_camera`. `virtual_camera_processor` gains an in-memory cache keyed by original `camera_id`; each cache entry stores maps, virtual intrinsic/extrinsic, dist data, and its task metadata. The compatibility pipeline builds caches once, saves maps/json once, then internally traverses original cameras and images, invoking the per-frame API exactly like the future external caller.

**Tech Stack:** C++17, OpenCV, Eigen, CMake tests.

---

### Task 1: Define Test-Driven Public API

**Files:**
- Modify: `include/virtual_camera/virtual_camera_processor.h`
- Modify: `tests/test_virtual_camera_processor.cpp`
- Modify: `src/virtual_camera_processor.cpp`

- [ ] **Step 1: Add tests first**

Add tests proving:
- `BuildVirtualCameraCache()` groups multiple virtual tasks under their source `camera_id`.
- `ProcessVirtualCameraFrame()` returns one remapped frame per cached virtual task for the incoming `camera_id`.
- Unknown `camera_id` returns no frames.

- [ ] **Step 2: Run test and verify RED**

Run:

```bash
cmake --build /tmp/virtual_camera_rt024_rename_build_clean --target test_virtual_camera_processor -j2
```

Expected before implementation: compilation fails because new API does not exist.

### Task 2: Implement Cache and Frame API

**Files:**
- Modify: `include/virtual_camera/virtual_camera_processor.h`
- Modify: `src/virtual_camera_processor.cpp`

- [ ] **Step 1: Add cache/result structs**

Expose:

```cpp
struct VirtualCameraCacheEntry;
struct VirtualCameraCache;
struct VirtualCameraFrameResult;
```

- [ ] **Step 2: Implement cache build**

Implement:

```cpp
VirtualCameraCache BuildVirtualCameraCache(const PipelineConfig& config);
```

- [ ] **Step 3: Implement per-frame processing**

Implement:

```cpp
std::vector<VirtualCameraFrameResult> ProcessVirtualCameraFrame(
    const VirtualCameraCache& cache, int camera_id, const cv::Mat& image);
```

### Task 3: Implement Artifact Save Functions

**Files:**
- Modify: `include/virtual_camera/virtual_camera_processor.h`
- Modify: `src/virtual_camera_processor.cpp`

- [ ] **Step 1: Save virtual cache artifacts**

Implement:

```cpp
void SaveVirtualCameraCacheArtifacts(const PipelineConfig& config,
                                     const VirtualCameraCache& cache);
```

This saves maps and virtual camera json from cache without processing images.

- [ ] **Step 2: Save virtual frame outputs**

Implement a private helper that writes `VirtualCameraFrameResult` images to the existing output layout.

### Task 4: Refactor Compatibility Pipeline

**Files:**
- Modify: `src/virtual_camera_processor.cpp`

- [ ] **Step 1: Preserve output validation**

Keep duplicate output validation before parallel image writes.

- [ ] **Step 2: Change traversal order**

`RunVirtualCameraPipeline()` builds cache once, saves artifacts once, then traverses by source camera task and image, calling `ProcessVirtualCameraFrame()` for each frame.

### Task 5: Verify

**Files:**
- No source changes.

- [ ] **Step 1: Build**

Run:

```bash
cmake -S . -B /tmp/virtual_camera_decouple_build
cmake --build /tmp/virtual_camera_decouple_build -j2
```

- [ ] **Step 2: Unit tests**

Run all `/tmp/virtual_camera_decouple_build/test_*`.

- [ ] **Step 3: Dataset regression**

Run:

```bash
/tmp/virtual_camera_decouple_build/virtual_camera_tool \
  --dataset_root /workspace/GACRT024_1754812994 \
  --config_path configs/config_rt024_parallel_run.json \
  --output_root /tmp/virtual_camera_decouple_output \
  --debug \
  --golden_root /workspace/GACRT024_1754812994
```

Expected: `verification passed`.
