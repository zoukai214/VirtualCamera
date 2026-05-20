# Rectify Virtual Camera Packaging Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为当前项目增加基于 CMake `install()` 的打包能力，产出 `build/rectify_virtual_camera` 独立运行包，并默认使用并行配置与包内脚本入口。

**Architecture:** 打包逻辑集中放在顶层 `CMakeLists.txt`，通过 `install(TARGETS ...)`、`install(FILES ...)` 和目录安装规则生成目标包。验证逻辑使用单独的 shell 脚本执行构建、安装和目录内容检查，避免把打包校验塞进现有 C++ 业务测试。文档层在 `README.md` 增补打包与下游运行方式。

**Tech Stack:** CMake 3.10, Bash, C++17 existing targets, vendored OpenCV, yaml-cpp

---

## File Structure

- Modify: `CMakeLists.txt`
  - 增加 `install()` 打包规则
  - 创建 `output/x86/lib` 兼容目录
- Create: `image_virtual.bash`
  - 作为包根目录运行入口
  - 只调用包内 `./virtual_camera_tool ./config.json`
- Create: `scripts/verify_rectify_package.sh`
  - 负责构建、安装和检查打包结果
- Modify: `README.md`
  - 增补打包命令和包内运行说明

### Task 1: Add Packaging Verification Script First

**Files:**
- Create: `scripts/verify_rectify_package.sh`
- Test: `scripts/verify_rectify_package.sh`

- [ ] **Step 1: Write the failing verification script**

Create `scripts/verify_rectify_package.sh` with this content:

```bash
#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
package_root="${repo_root}/build/rectify_virtual_camera"

cd "${repo_root}"

cmake -S . -B build
cmake --build build -j"$(nproc)"
rm -rf "${package_root}"
cmake --install build --prefix "${package_root}"

test -f "${package_root}/virtual_camera_tool"
test -f "${package_root}/config.json"
test -f "${package_root}/image_virtual.bash"
test -d "${package_root}/output/x86/lib"

cmp "${package_root}/config.json" "${repo_root}/configs/config_rt024_parallel_run.json"

if grep -q 'build/' "${package_root}/image_virtual.bash"; then
  echo "image_virtual.bash unexpectedly references build/ path" >&2
  exit 1
fi

grep -q '\./virtual_camera_tool \./config.json' "${package_root}/image_virtual.bash"

echo "rectify package verification passed"
```

- [ ] **Step 2: Run the verification script to confirm it fails before implementation**

Run:

```bash
bash scripts/verify_rectify_package.sh
```

Expected: FAIL because `cmake --install` does not yet install `virtual_camera_tool`, `config.json`, `image_virtual.bash`, or `output/x86/lib`.

- [ ] **Step 3: Commit the failing test harness**

Run:

```bash
git add scripts/verify_rectify_package.sh
git commit -m "test: add rectify package verification script"
```

### Task 2: Implement Package Install Rules and Runtime Script

**Files:**
- Modify: `CMakeLists.txt`
- Create: `image_virtual.bash`
- Test: `scripts/verify_rectify_package.sh`

- [ ] **Step 1: Write the package runtime script**

Create `image_virtual.bash` with this content:

```bash
#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
cd "${script_dir}"

./virtual_camera_tool ./config.json
```

- [ ] **Step 2: Update `CMakeLists.txt` to install the binary, config, script, and compatibility directory**

Append these blocks near the end of `CMakeLists.txt` after the existing test target declarations:

```cmake
set(RECTIFY_PACKAGE_OUTPUT_X86_LIB_DIR output/x86/lib)

if(TARGET virtual_camera_tool)
    install(TARGETS virtual_camera_tool
        RUNTIME DESTINATION .
    )
endif()

install(FILES
    ${PROJECT_SOURCE_DIR}/configs/config_rt024_parallel_run.json
    DESTINATION .
    RENAME config.json
)

install(PROGRAMS
    ${PROJECT_SOURCE_DIR}/image_virtual.bash
    DESTINATION .
)

install(DIRECTORY
    ${PROJECT_SOURCE_DIR}/third_party/opencv/lib/
    DESTINATION ${RECTIFY_PACKAGE_OUTPUT_X86_LIB_DIR}
    FILES_MATCHING
    PATTERN "*.so"
    PATTERN "*.so.*"
)

install(CODE
    "file(MAKE_DIRECTORY \"\$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/${RECTIFY_PACKAGE_OUTPUT_X86_LIB_DIR}\")"
)
```

Implementation notes for the engineer:

- Keep the existing compile flags untouched.
- `install(PROGRAMS ...)` preserves executable permission for `image_virtual.bash`.
- The `install(DIRECTORY ...)` clause may copy nothing if vendored OpenCV only contains static archives, which is acceptable.
- `install(CODE ...)` ensures `output/x86/lib` exists even when no `.so` files are installed.

- [ ] **Step 3: Run the verification script to confirm packaging now passes**

Run:

```bash
bash scripts/verify_rectify_package.sh
```

Expected: PASS with final line:

```text
rectify package verification passed
```

- [ ] **Step 4: Inspect runtime dependencies to see whether non-system shared libraries need to be packaged**

Run:

```bash
ldd build/virtual_camera_tool
```

Expected:

- `linux-vdso.so.1`, `libstdc++.so.*`, `libm.so.*`, `libgcc_s.so.*`, `libc.so.*` and similar system libraries may appear
- if `libyaml-cpp.so*` appears from a non-system location that downstream will not have, add a follow-up edit in `CMakeLists.txt` to install that `.so` into `output/x86/lib`

If a non-system runtime dependency is found, extend the install rules before moving on. Re-run:

```bash
bash scripts/verify_rectify_package.sh
```

Expected: still PASS.

- [ ] **Step 5: Commit the packaging implementation**

Run:

```bash
git add CMakeLists.txt image_virtual.bash scripts/verify_rectify_package.sh
git commit -m "feat: add rectify package install layout"
```

### Task 3: Document Package Build and Downstream Usage

**Files:**
- Modify: `README.md`
- Test: `README.md`

- [ ] **Step 1: Update the README packaging section**

Add a new section after the build instructions in `README.md` with content equivalent to:

```markdown
## 打包

生成下游可直接运行的独立目录：

```bash
cmake -S . -B build
cmake --build build -j"$(nproc)"
rm -rf build/rectify_virtual_camera
cmake --install build --prefix /workspace/VirtualCamera/build/rectify_virtual_camera
```

打包结果目录：

```bash
build/rectify_virtual_camera
```

其中默认配置为并行版本：

```bash
configs/config_rt024_parallel_run.json
```

下游进入包目录后可直接运行：

```bash
cd build/rectify_virtual_camera
bash image_virtual.bash
```
```

- [ ] **Step 2: Re-run the verification script after the README update**

Run:

```bash
bash scripts/verify_rectify_package.sh
```

Expected:

```text
rectify package verification passed
```

- [ ] **Step 3: Review the package tree manually for final sanity**

Run:

```bash
find build/rectify_virtual_camera -maxdepth 3 | sort
```

Expected to include at least:

```text
build/rectify_virtual_camera
build/rectify_virtual_camera/config.json
build/rectify_virtual_camera/image_virtual.bash
build/rectify_virtual_camera/output
build/rectify_virtual_camera/output/x86
build/rectify_virtual_camera/output/x86/lib
build/rectify_virtual_camera/virtual_camera_tool
```

- [ ] **Step 4: Commit the documentation update**

Run:

```bash
git add README.md
git commit -m "docs: add rectify package usage"
```

## Verification Commands

Run the full verification sequence before claiming completion:

```bash
bash scripts/verify_rectify_package.sh
ldd build/virtual_camera_tool
find build/rectify_virtual_camera -maxdepth 3 | sort
git status --short
```

Expected:

- packaging verification passes
- runtime dependency situation is understood and handled
- output tree matches the spec
- no unintended files are modified

## Spec Coverage Check

- 独立运行包输出到 `build/rectify_virtual_camera`: covered by Task 2 and verification script
- 目录层级对齐 `/workspace/rectify_virtual_camera`: covered by Task 2 `output/x86/lib` and Task 3 manual tree check
- 默认并行配置: covered by Task 1 `cmp` check and Task 2 install rename
- `image_virtual.bash` 不含 `build/` 路径: covered by Task 1 grep check and Task 2 script content
- 不改业务逻辑和编译选项: covered by limiting edits to install rules, script, docs, and verification
- 新增测试验证打包产物: covered by Task 1 verification script

