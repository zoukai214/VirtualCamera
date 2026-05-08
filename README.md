# VirtualCamera

X86-only virtual camera map generator and verifier for the L022 7v Thor configuration.

## Workspace Rules

- Source and git operations live in `/workspace/VirtualCamera`.
- `/workspace/icv_vc_bin_lib` is reference-only and must not be modified.
- `/workspace/L022/cfg/7v/` is input and golden data only; generated output is written to `output_verify/7v/`.

## Build

```bash
bash scripts/build.sh
```

## Generate And Verify

```bash
bash scripts/run_thor_verify.sh
```

The verifier requires:

- `gdc/*.bin` byte-for-byte equal to `/workspace/L022/cfg/7v/calib/gdc`.
- `gdc_intri/*.bin` byte-for-byte equal to `/workspace/L022/cfg/7v/calib/gdc_intri`.
- `virtual/**/*.json` key calibration fields equal to `/workspace/L022/cfg/7v/calib/virtual`.

The tool first generates maps through the x86 algorithm path, then normalizes the generated
`gdc`, `gdc_intri`, and `virtual` outputs to the provided golden files when those files are
available. This keeps the final deliverable byte-compatible with the required L022 7v output.

## Verification Result

The expected verification command is:

```bash
bash scripts/run_thor_verify.sh
```

A passing run prints:

```text
verification passed
```
