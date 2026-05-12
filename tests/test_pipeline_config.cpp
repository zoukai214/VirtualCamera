#include "virtual_camera/pipeline_config.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {

void WriteText(const std::string& path, const std::string& content) {
  std::ofstream output(path);
  output << content;
}

void Expect(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void TestLoadPipelineConfigSuccess() {
  const std::string root = "build/test_tmp/pipeline_config_success";
  std::filesystem::create_directories(root);
  const std::string path = root + "/config.json";
  WriteText(path, R"json({
    "dataset_root": "/workspace/GACRT024_1754812994",
    "golden_root": "/workspace/GACRT024_1754812994",
    "output_root": "build/rt024_output",
    "conf_dir_path": "calib_extract/",
    "image_dir_path": "image_raw/",
    "vc_image_dir_path": "image_virtual_camera/",
    "undistort_image_dir_path": "image_undistortion/",
    "vc_conf_dir_path": "calib_virtual_camera/",
    "undistort_conf_dir_path": "calib_undistortion/",
    "vc_gdcbin_dir_path": "vc_gdcbin_dir_path/",
    "showinfo": 1,
    "showdir": 0,
    "process_virtual_camera": 1,
    "process_undistort": 1,
    "task_parallelism": 2,
    "undistort_parallelism": 3,
    "virtual_camera_parallelism": 4,
    "undistort_image": 0,
    "distort_model": 0,
    "save_virtual_json": 1,
    "save_undistort_json": 1,
    "conf_type": 3,
    "virtual_camera_configs": [{
      "desc": "front wide -> front wide 110",
      "conf_json": "calib_camera_front_wide_to_car.json",
      "conf_intri_key": "camera-front-wide",
      "conf_extri_key": "camera-front-wide-to-car",
      "image_dir": "front_wide/",
      "save_dir": "front_wide_110/",
      "calib_json": "calib_cam_front_wide_fov110.json",
      "file_prefix": "fw110",
      "camera_id": 1,
      "image_width": 3840,
      "image_height": 2160,
      "fov": 120,
      "undistort_image": 1,
      "vc_mapX_name": "fw110_vc_mapX.bin",
      "vc_mapY_name": "fw110_vc_mapY.bin",
      "src2vc_mapX_name": "fw110_src2vc_mapX.bin",
      "src2vc_mapY_name": "fw110_src2vc_mapY.bin",
      "new_intrinsic": {
        "fov": 110,
        "focal_u": 358.5,
        "center_u": 512.0,
        "focal_v": 358.5,
        "center_v": 256.0,
        "image_width": 1024,
        "image_height": 512
      },
      "new_extrinsics": {
        "pitch": 0.0,
        "roll": 0.0,
        "yaw": 0.0,
        "x": 1.95,
        "y": -0.04,
        "z": 1.54
      }
    }],
    "undistort_configs": [{
      "conf_json": "calib_camera_front_wide_to_car.json",
      "intri_key": "camera-front-wide",
      "extri_key": "camera-front-wide-to-car",
      "image_dir": "front_wide/",
      "new_intrinsic": {
        "focal_u": 1000.0,
        "center_u": 1920.0,
        "focal_v": 1000.0,
        "center_v": 1080.0,
        "image_width": 3840,
        "image_height": 2160,
        "center": 1
      }
    }]
  })json");

  const vc::PipelineConfig config = vc::LoadPipelineConfig(path);
  Expect(config.dataset_root == "/workspace/GACRT024_1754812994", "dataset_root");
  Expect(config.golden_root == "/workspace/GACRT024_1754812994", "golden_root");
  Expect(config.output_root == "build/rt024_output", "output_root");
  Expect(config.virtual_tasks.size() == 1, "virtual task count");
  Expect(config.undistort_tasks.size() == 1, "undistort task count");
  Expect(config.task_parallelism == 2, "task_parallelism");
  Expect(config.undistort_parallelism == 3, "undistort_parallelism");
  Expect(config.virtual_camera_parallelism == 4, "virtual_camera_parallelism");
  Expect(config.virtual_tasks.front().save_dir == "front_wide_110/", "save_dir");
  Expect(config.virtual_tasks.front().calib_json == "calib_cam_front_wide_fov110.json",
         "calib_json");
  Expect(config.virtual_tasks.front().file_prefix == "fw110", "file_prefix");
  Expect(config.virtual_tasks.front().src2vc_mapx_name == "fw110_src2vc_mapX.bin",
         "src2vc_mapx_name");
  Expect(config.undistort_tasks.front().image_dir == "front_wide/", "image_dir");
}

void TestLoadPipelineConfigMissingField() {
  const std::string root = "build/test_tmp/pipeline_config_missing";
  std::filesystem::create_directories(root);
  const std::string path = root + "/config.json";
  WriteText(path, R"json({
    "dataset_root": "/workspace/GACRT024_1754812994",
    "virtual_camera_configs": [],
    "undistort_configs": []
  })json");

  bool thrown = false;
  try {
    static_cast<void>(vc::LoadPipelineConfig(path));
  } catch (const std::runtime_error& error) {
    thrown = std::string(error.what()).find("conf_dir_path") != std::string::npos;
  }
  Expect(thrown, "missing field should mention conf_dir_path");
}

void TestLoadPipelineConfigParallelismDefaults() {
  const std::string root = "build/test_tmp/pipeline_config_parallel_defaults";
  std::filesystem::create_directories(root);
  const std::string path = root + "/config.json";
  WriteText(path, R"json({
    "dataset_root": "/workspace/GACRT024_1754812994",
    "golden_root": "/workspace/GACRT024_1754812994",
    "output_root": "build/rt024_output",
    "conf_dir_path": "calib_extract/",
    "image_dir_path": "image_raw/",
    "vc_image_dir_path": "image_virtual_camera/",
    "undistort_image_dir_path": "image_undistortion/",
    "vc_conf_dir_path": "calib_virtual_camera/",
    "undistort_conf_dir_path": "calib_undistortion/",
    "vc_gdcbin_dir_path": "vc_gdcbin_dir_path/",
    "virtual_camera_configs": [],
    "undistort_configs": []
  })json");

  const vc::PipelineConfig config = vc::LoadPipelineConfig(path);
  Expect(config.task_parallelism == 1, "default task_parallelism");
  Expect(config.undistort_parallelism == 1, "default undistort_parallelism");
  Expect(config.virtual_camera_parallelism == 1,
         "default virtual_camera_parallelism");
}

}  // namespace

int main() {
  TestLoadPipelineConfigSuccess();
  TestLoadPipelineConfigMissingField();
  TestLoadPipelineConfigParallelismDefaults();
  return 0;
}
