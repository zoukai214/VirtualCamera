# VirtualCamera

RT024 data pipeline tool for undistort and virtual camera generation.

## Build

```bash
bash scripts/build.sh
```

## Run RT024 Pipeline

```bash
LD_LIBRARY_PATH=/workspace/gen_vc_bin_lib_test/deps/x86/gen_vc_map_lib/lib:${LD_LIBRARY_PATH:-} \
./build/virtual_camera_tool configs/config_rt024.json
```

默认配置：

- 输入数据根目录：`/workspace/GACRT024_1754812994`
- 真值目录：`/workspace/GACRT024_1754812994`
- 输出目录：`build/rt024_output`

程序会生成：

- `calib_undistortion`
- `calib_virtual_camera`
- `image_undistortion`
- `image_virtual_camera`
- `vc_gdcbin_dir_path`

## Verification

当前自动校验范围：

- `vc_gdcbin_dir_path` 中的虚拟相机映射表逐字节一致
- `calib_undistortion` 中的参数 JSON 一致
- `calib_virtual_camera` 中的参数 JSON 一致

图像当前只要求生成，不做自动像素比对。

校验通过时输出：

```text
verification passed
```
