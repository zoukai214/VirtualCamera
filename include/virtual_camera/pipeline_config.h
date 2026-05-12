#pragma once

#include <string>
#include <vector>

namespace vc {

struct OutputPathConfig {
  std::string dataset_root;
  std::string conf_dir_path;
  std::string image_dir_path;
  std::string vc_image_dir_path;
  std::string undistort_image_dir_path;
  std::string vc_conf_dir_path;
  std::string undistort_conf_dir_path;
  std::string vc_gdcbin_dir_path;
};

struct NewIntrinsicConfig {
  double fov = 0.0;
  double focal_u = 0.0;
  double center_u = 0.0;
  double focal_v = 0.0;
  double center_v = 0.0;
  int image_width = 0;
  int image_height = 0;
  int center = 1;
};

struct NewExtrinsicsConfig {
  double pitch = 0.0;
  double roll = 0.0;
  double yaw = 0.0;
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
};

struct VirtualCameraTaskConfig {
  std::string desc;
  std::string conf_json;
  std::string conf_intri_key;
  std::string conf_extri_key;
  std::string image_dir;
  std::string save_dir;
  std::string calib_json;
  std::string file_prefix;
  std::string vc_mapx_name;
  std::string vc_mapy_name;
  std::string src2vc_mapx_name;
  std::string src2vc_mapy_name;
  int camera_id = 0;
  int image_width = 0;
  int image_height = 0;
  int fov = 0;
  int undistort_image = 0;
  NewIntrinsicConfig new_intrinsic;
  NewExtrinsicsConfig new_extrinsics;
};

struct UndistortTaskConfig {
  std::string conf_json;
  std::string intri_key;
  std::string extri_key;
  std::string image_dir;
  NewIntrinsicConfig new_intrinsic;
};

struct PipelineConfig {
  OutputPathConfig paths;
  int showinfo = 0;
  int showdir = 0;
  int process_virtual_camera = 0;
  int process_undistort = 0;
  int task_parallelism = 1;
  int undistort_parallelism = 1;
  int virtual_camera_parallelism = 1;
  int undistort_image = 0;
  int distort_model = 0;
  int save_virtual_json = 0;
  int save_undistort_json = 0;
  int conf_type = 0;
  std::string dataset_root;
  std::string golden_root;
  std::string output_root;
  std::vector<VirtualCameraTaskConfig> virtual_tasks;
  std::vector<UndistortTaskConfig> undistort_tasks;
};

PipelineConfig LoadPipelineConfig(const std::string& config_path);

}  // namespace vc
