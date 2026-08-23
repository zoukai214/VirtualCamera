import json
import shutil
import sys
from pathlib import Path


def expect(condition, message):
    if not condition:
        raise RuntimeError(message)


def make_test_root(name):
    root = Path("build/test_tmp/python_pipeline_orchestrator") / name
    if root.exists():
        shutil.rmtree(root)
    root.mkdir(parents=True)
    return root


def write_tiny_calibration_json(path):
    data = {
        "camera-front-wide": {
            "param": {
                "cam_matrix": {
                    "data": [
                        [1909.0533447265625, 0, 1902.6021728515625],
                        [0, 1909.345458984375, 1088.849609375],
                        [0, 0, 1],
                    ]
                },
                "cam_dist": {
                    "data": [[
                        0.27403417229652405,
                        -0.018113205209374428,
                        -2.64449499809416e-05,
                        1.5385621736641042e-05,
                        0.0010280556743964553,
                        0.6341700553894043,
                        0,
                        0,
                    ]]
                },
                "width": 3840,
                "height": 2160,
            }
        },
        "camera-front-wide-to-car": {
            "param": {
                "sensor_calib": {
                    "data": [
                        [-0.010597652684796122, -0.010371027993724968, 0.9998900597245306, 1.9657248981983317],
                        [-0.9999331002492209, 0.004745092656345318, -0.010548891963789553, -0.061965364385173805],
                        [-0.00463516812569198, -0.9999349608219705, -0.010420621018465637, 1.5531924098970122],
                        [0, 0, 0, 1],
                    ]
                }
            }
        },
    }
    path.write_text(json.dumps(data), encoding="utf-8")


def write_tiny_ppm(path):
    width = 16
    height = 8
    with path.open("wb") as output:
        output.write(f"P6\n{width} {height}\n255\n".encode("ascii"))
        for row in range(height):
            for col in range(width):
                output.write(bytes((col * 7, row * 13, (row + col) * 5)))


def make_dataset(root):
    dataset_root = root / "dataset"
    conf_dir = dataset_root / "calib_extract"
    image_dir = dataset_root / "image_raw" / "front_wide"
    conf_dir.mkdir(parents=True)
    image_dir.mkdir(parents=True)
    write_tiny_calibration_json(conf_dir / "calib_camera_front_wide_to_car.json")
    write_tiny_ppm(image_dir / "synthetic_front_wide.ppm")
    return dataset_root


def write_config_json(root):
    config_path = root / "config.json"
    config = {
        "conf_dir_path": "calib_extract",
        "image_dir_path": "image_raw",
        "vc_image_dir_path": "image_virtual_camera",
        "undistort_image_dir_path": "image_undistortion",
        "vc_conf_dir_path": "calib_virtual_camera",
        "undistort_conf_dir_path": "calib_undistortion",
        "vc_gdcbin_dir_path": "vc_gdcbin_dir_path",
        "showinfo": 0,
        "process_virtual_camera": 1,
        "process_undistort": 1,
        "undistort_image": 0,
        "distort_model": 0,
        "virtual_camera_configs": [{
            "desc": "front wide tiny virtual",
            "conf_json": "calib_camera_front_wide_to_car.json",
            "conf_intri_key": "camera-front-wide",
            "conf_extri_key": "camera-front-wide-to-car",
            "image_dir": "front_wide/",
            "save_dir": "front_wide_110/",
            "calib_json": "calib_cam_front_wide_fov110.json",
            "file_prefix": "fw110",
            "vc_mapX_name": "fw110_vc_mapX.bin",
            "vc_mapY_name": "fw110_vc_mapY.bin",
            "src2vc_mapX_name": "fw110_src2vc_mapX.bin",
            "src2vc_mapY_name": "fw110_src2vc_mapY.bin",
            "camera_id": 1,
            "image_width": 3840,
            "image_height": 2160,
            "fov": 120,
            "undistort_image": 1,
            "new_intrinsic": {
                "fov": 110.0,
                "focal_u": 8.0,
                "center_u": 512.0,
                "focal_v": 8.0,
                "center_v": 256.0,
                "image_width": 16,
                "image_height": 8,
                "center": 1,
            },
            "new_extrinsics": {
                "pitch": 0.0,
                "roll": 0.0,
                "yaw": 0.0,
                "x": 0.0,
                "y": 0.0,
                "z": 0.0,
            },
        }],
        "undistort_configs": [{
            "conf_json": "calib_camera_front_wide_to_car.json",
            "intri_key": "camera-front-wide",
            "extri_key": "camera-front-wide-to-car",
            "image_dir": "front_wide/",
            "new_intrinsic": {
                "fov": 110.0,
                "focal_u": 8.0,
                "center_u": 512.0,
                "focal_v": 8.0,
                "center_v": 256.0,
                "image_width": 16,
                "image_height": 8,
                "center": 1,
            },
        }],
        "task_parallelism": 1,
        "undistort_parallelism": 1,
        "virtual_camera_parallelism": 1,
    }
    config_path.write_text(json.dumps(config), encoding="utf-8")
    return config_path


def main():
    sys.path.insert(0, str(Path("build/python").resolve()))
    import virtual_camera

    root = make_test_root("virtual_camera_workflow")
    dataset_root = make_dataset(root)
    config_path = write_config_json(root)
    orchestrator = virtual_camera.PipelineOrchestrator(
        str(config_path), str(dataset_root), "all"
    )

    sources = orchestrator.virtual_source_inputs()
    expect(sources == [{"camera_id": 1, "image_dir": "front_wide/"}],
           "virtual source inputs should be available from Python")
    undistort_sources = orchestrator.undistort_source_inputs()
    expect(undistort_sources == [{"camera_id": 1, "image_dir": "front_wide/"}],
           "undistort source inputs should be available from Python")

    orchestrator.save_virtual_camera_artifacts(str(root))
    orchestrator.save_undistort_artifacts(str(root))
    source_image = dataset_root / "image_raw" / "front_wide" / "synthetic_front_wide.ppm"
    orchestrator.process_and_save_virtual_camera_frame(
        1, str(source_image), str(root)
    )
    orchestrator.process_and_save_undistort_frame(
        1, str(source_image), str(root)
    )

    expect((root / "calib_virtual_camera" / "calib_cam_front_wide_fov110.json").exists(),
           "Python workflow should save virtual json")
    expect((root / "calib_undistortion" / "calib_camera_front_wide_to_car.json").exists(),
           "Python workflow should save undistort json")
    expect((root / "vc_gdcbin_dir_path" / "fw110_vc_mapX.bin").exists(),
           "Python workflow should save virtual map")
    expect((root / "image_virtual_camera" / "front_wide_110" / "fw110_synthetic_front_wide.ppm").exists(),
           "Python workflow should save virtual image")
    expect((root / "image_undistortion" / "front_wide" / "synthetic_front_wide.ppm").exists(),
           "Python workflow should save undistort image")


if __name__ == "__main__":
    main()
