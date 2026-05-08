#include "virtual_camera/task_builder.h"

#include "virtual_camera/json_utils.h"

#include <filesystem>
#include <set>

namespace vc {
namespace {

VirtualParam ParseVirtualParam(const nlohmann::json& camera) {
  VirtualParam param;
  param.virtual_width = camera.at("new_intrinsic").at("image_width").get<int>();
  param.virtual_height = camera.at("new_intrinsic").at("image_height").get<int>();
  param.virtual_fov = camera.at("new_intrinsic").at("fov").get<double>();
  param.virtual_fu = camera.at("new_intrinsic").at("focal_u").get<double>();
  param.virtual_fv = camera.at("new_intrinsic").at("focal_v").get<double>();
  param.virtual_cx = camera.at("new_intrinsic").at("center_u").get<double>();
  param.virtual_cy = camera.at("new_intrinsic").at("center_v").get<double>();
  param.virtual_yaw = camera.at("new_extrinsics").at("yaw").get<double>();
  param.virtual_roll = camera.at("new_extrinsics").at("roll").get<double>();
  param.virtual_pitch = camera.at("new_extrinsics").at("pitch").get<double>();
  param.image_width = camera.at("image_width").get<int>();
  param.image_height = camera.at("image_height").get<int>();
  param.fov = camera.at("fov").get<double>();
  param.camera_id = camera.at("camera_id").get<int>();
  param.bin_name = camera.at("bin_name").get<std::string>();
  return param;
}

CameraTask ParseTask(const nlohmann::json& camera, TaskType type) {
  CameraTask task;
  task.type = type;
  task.description = camera.value("desc", std::string());
  task.conf_json = camera.at("conf_json").get<std::string>();
  task.intri_json = camera.at("intri_json").get<std::string>();
  task.extri_json = camera.at("extri_json").get<std::string>();
  task.bin_name = camera.at("bin_name").get<std::string>();
  task.undistort_bin_name = camera.value("undis_bin_name", std::string());
  task.camera_id = camera.at("camera_id").get<int>();
  task.image_width = camera.at("image_width").get<int>();
  task.image_height = camera.at("image_height").get<int>();
  task.source_fov = camera.at("fov").get<int>();
  task.virtual_param = ParseVirtualParam(camera);
  return task;
}

}  // namespace

std::vector<CameraTask> BuildThorTasks(const nlohmann::json& config) {
  std::vector<CameraTask> tasks;
  std::set<int> undistort_camera_ids;

  for (const auto& camera : config.at("virtual_camera_configs")) {
    tasks.push_back(ParseTask(camera, TaskType::kVirtual));
    const int camera_id = camera.at("camera_id").get<int>();
    if (undistort_camera_ids.insert(camera_id).second) {
      CameraTask undistort_task = ParseTask(camera, TaskType::kUndistort);
      undistort_task.bin_name = camera.at("undis_bin_name").get<std::string>();
      tasks.push_back(undistort_task);
    }
  }

  for (const auto& camera : config.at("virtual_init_camera_config")) {
    tasks.push_back(ParseTask(camera, TaskType::kResize));
  }

  return tasks;
}

CalibrationParam LoadCalibration(const std::string& input_root, const CameraTask& task) {
  const std::filesystem::path calib_root =
      std::filesystem::path(input_root) / "calib";
  const auto intri_json = ReadJson((calib_root / task.intri_json).string());
  const auto extri_json = ReadJson((calib_root / task.extri_json).string());

  CalibrationParam calibration;
  calibration.extrinsic_matrix =
      JsonToMatrix4d(extri_json.at("value0").at("param").at("sensor_calib").at("data"));
  calibration.intrinsic_matrix =
      JsonToMatrix3d(intri_json.at("value0").at("param").at("cam_K").at("data"));
  calibration.dist_data =
      JsonToVector(intri_json.at("value0").at("param").at("cam_dist").at("data"));
  calibration.image_width = task.image_width;
  calibration.image_height = task.image_height;
  calibration.fov = task.source_fov;
  return calibration;
}

}  // namespace vc
