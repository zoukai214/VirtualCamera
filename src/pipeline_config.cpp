#include "virtual_camera/pipeline_config.h"

#include "virtual_camera/json_utils.h"

#include <stdexcept>
#include <string>

namespace vc {
namespace {

template <typename T>
T Required(const nlohmann::json& json, const char* key) {
  if (!json.contains(key)) {
    throw std::runtime_error(std::string("missing required field: ") + key);
  }
  return json.at(key).get<T>();
}

NewIntrinsicConfig ParseNewIntrinsic(const nlohmann::json& json) {
  NewIntrinsicConfig cfg;
  cfg.fov = json.value("fov", 0.0);
  cfg.focal_u = Required<double>(json, "focal_u");
  cfg.center_u = Required<double>(json, "center_u");
  cfg.focal_v = Required<double>(json, "focal_v");
  cfg.center_v = Required<double>(json, "center_v");
  cfg.image_width = Required<int>(json, "image_width");
  cfg.image_height = Required<int>(json, "image_height");
  cfg.center = json.value("center", 1);
  return cfg;
}

NewExtrinsicsConfig ParseNewExtrinsics(const nlohmann::json& json) {
  NewExtrinsicsConfig cfg;
  cfg.pitch = Required<double>(json, "pitch");
  cfg.roll = Required<double>(json, "roll");
  cfg.yaw = Required<double>(json, "yaw");
  cfg.x = Required<double>(json, "x");
  cfg.y = Required<double>(json, "y");
  cfg.z = Required<double>(json, "z");
  return cfg;
}

}  // namespace

PipelineConfig LoadPipelineConfig(const std::string& config_path) {
  const auto json = ReadJson(config_path);

  PipelineConfig cfg;
  cfg.dataset_root = Required<std::string>(json, "dataset_root");
  cfg.golden_root = json.value("golden_root", cfg.dataset_root);
  cfg.output_root = json.value("output_root", cfg.dataset_root);
  cfg.paths.dataset_root = cfg.dataset_root;
  cfg.paths.conf_dir_path = Required<std::string>(json, "conf_dir_path");
  cfg.paths.image_dir_path = Required<std::string>(json, "image_dir_path");
  cfg.paths.vc_image_dir_path = Required<std::string>(json, "vc_image_dir_path");
  cfg.paths.undistort_image_dir_path =
      Required<std::string>(json, "undistort_image_dir_path");
  cfg.paths.vc_conf_dir_path = Required<std::string>(json, "vc_conf_dir_path");
  cfg.paths.undistort_conf_dir_path =
      Required<std::string>(json, "undistort_conf_dir_path");
  cfg.paths.vc_gdcbin_dir_path = Required<std::string>(json, "vc_gdcbin_dir_path");
  cfg.showinfo = json.value("showinfo", 0);
  cfg.showdir = json.value("showdir", 0);
  cfg.process_virtual_camera = json.value("process_virtual_camera", 0);
  cfg.process_undistort = json.value("process_undistort", 0);
  cfg.task_parallelism = json.value("task_parallelism", 1);
  cfg.undistort_parallelism = json.value("undistort_parallelism", 1);
  cfg.virtual_camera_parallelism = json.value("virtual_camera_parallelism", 1);
  cfg.undistort_image = json.value("undistort_image", 0);
  cfg.distort_model = json.value("distort_model", 0);
  cfg.save_virtual_json = json.value("save_virtual_json", 0);
  cfg.save_undistort_json = json.value("save_undistort_json", 0);
  cfg.conf_type = json.value("conf_type", 0);

  for (const auto& task_json : Required<nlohmann::json>(json, "virtual_camera_configs")) {
    VirtualCameraTaskConfig task;
    task.desc = task_json.value("desc", std::string());
    task.conf_json = Required<std::string>(task_json, "conf_json");
    task.conf_intri_key = Required<std::string>(task_json, "conf_intri_key");
    task.conf_extri_key = Required<std::string>(task_json, "conf_extri_key");
    task.image_dir = Required<std::string>(task_json, "image_dir");
    task.save_dir = Required<std::string>(task_json, "save_dir");
    task.calib_json = Required<std::string>(task_json, "calib_json");
    task.file_prefix = Required<std::string>(task_json, "file_prefix");
    task.vc_mapx_name = Required<std::string>(task_json, "vc_mapX_name");
    task.vc_mapy_name = Required<std::string>(task_json, "vc_mapY_name");
    task.src2vc_mapx_name = Required<std::string>(task_json, "src2vc_mapX_name");
    task.src2vc_mapy_name = Required<std::string>(task_json, "src2vc_mapY_name");
    task.camera_id = Required<int>(task_json, "camera_id");
    task.image_width = Required<int>(task_json, "image_width");
    task.image_height = Required<int>(task_json, "image_height");
    task.fov = Required<int>(task_json, "fov");
    task.undistort_image = task_json.value("undistort_image", 0);
    task.new_intrinsic = ParseNewIntrinsic(task_json.at("new_intrinsic"));
    task.new_extrinsics = ParseNewExtrinsics(task_json.at("new_extrinsics"));
    cfg.virtual_tasks.push_back(task);
  }

  for (const auto& task_json : Required<nlohmann::json>(json, "undistort_configs")) {
    UndistortTaskConfig task;
    task.conf_json = Required<std::string>(task_json, "conf_json");
    task.intri_key = Required<std::string>(task_json, "intri_key");
    task.extri_key = Required<std::string>(task_json, "extri_key");
    task.image_dir = Required<std::string>(task_json, "image_dir");
    task.new_intrinsic = ParseNewIntrinsic(task_json.at("new_intrinsic"));
    cfg.undistort_tasks.push_back(task);
  }

  return cfg;
}

}  // namespace vc
