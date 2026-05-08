# Split 4V Config Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move the 4V verification YAML parameters out of `scripts/run_4v_verify.sh` into a standalone config file.

**Architecture:** Add `configs/config_4v.yaml` as the reusable 4V YAML configuration. Keep `scripts/run_4v_verify.sh` as a runner and verifier only.

**Tech Stack:** Bash, YAML, existing `virtual_camera_tool generate-4v`.

---

### Task 1: Split 4V Config From Runner

**Files:**
- Create: `configs/config_4v.yaml`
- Modify: `scripts/run_4v_verify.sh`

- [ ] **Step 1: Create config file**

Create `configs/config_4v.yaml` with the 4V input, output, stitching, image, and cylinder parameters currently embedded in the script. Use absolute project output paths under `/workspace/VirtualCamera/output_verify/4v`.

- [ ] **Step 2: Simplify runner**

Change `scripts/run_4v_verify.sh` so it sets `CONFIG_PATH="${ROOT_DIR}/configs/config_4v.yaml"`, removes the heredoc, and invokes:

```bash
"${ROOT_DIR}/build/virtual_camera_tool" generate-4v "${CONFIG_PATH}"
```

- [ ] **Step 3: Verify**

Run:

```bash
bash scripts/run_4v_verify.sh
bash scripts/run_thor_verify.sh
```

Expected output includes:

```text
4v verification passed
verification passed
```
