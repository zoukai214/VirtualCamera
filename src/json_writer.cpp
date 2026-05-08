#include "virtual_camera/json_writer.h"

#include "virtual_camera/json_utils.h"

#include <filesystem>

namespace vc {

void SaveVirtualJson(const std::string& input_root, const std::string& output_root,
                     const CameraTask& task, const Eigen::Matrix4d& virtual_extrinsic,
                     const Eigen::Matrix3d& virtual_intrinsic,
                     const std::vector<double>& dist_data) {
  const std::filesystem::path input_calib = std::filesystem::path(input_root) / "calib";
  nlohmann::json intri_json = ReadJson((input_calib / task.intri_json).string());
  nlohmann::json extri_json = ReadJson((input_calib / task.extri_json).string());
  const std::string camera_name = task.conf_json;

  auto& extri_data = extri_json["value0"]["param"]["sensor_calib"]["data"];
  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 3; ++col) {
      extri_data[row][col] = virtual_extrinsic(row, col);
    }
  }
  extri_json["value0"]["sensor_name"] = camera_name;

  intri_json["value0"]["param"]["img_new_w"] = task.virtual_param.virtual_width;
  intri_json["value0"]["param"]["img_new_h"] = task.virtual_param.virtual_height;

  auto& new_k = intri_json["value0"]["param"]["cam_K_new"]["data"];
  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 3; ++col) {
      new_k[row][col] = virtual_intrinsic(row, col);
    }
  }

  intri_json["value0"]["param"]["cam_dist"]["cols"] = dist_data.size();
  auto& dist = intri_json["value0"]["param"]["cam_dist"]["data"][0];
  dist = nlohmann::json::array();
  for (double value : dist_data) {
    dist.push_back(value);
  }
  intri_json["value0"]["sensor_name"] = camera_name;
  intri_json["value0"]["target_sensor_name"] = camera_name;

  const std::filesystem::path output_dir =
      std::filesystem::path(output_root) / "calib/virtual" / camera_name;
  WriteJson((output_dir / (camera_name + "-intrinsic.json")).string(), intri_json);
  WriteJson((output_dir / (camera_name + "-to-car_center-extrinsic.json")).string(), extri_json);
}

}  // namespace vc
