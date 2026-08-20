#include "virtual_camera/json_writer.h"

#include "virtual_camera/json_utils.h"

#include <filesystem>

namespace vc {
namespace {

nlohmann::json Matrix3dToJson(const Eigen::Matrix3d& matrix) {
  nlohmann::json rows = nlohmann::json::array();
  for (int row = 0; row < 3; ++row) {
    nlohmann::json cols = nlohmann::json::array();
    for (int col = 0; col < 3; ++col) {
      cols.push_back(matrix(row, col));
    }
    rows.push_back(cols);
  }
  return rows;
}

nlohmann::json Matrix4dToJson(const Eigen::Matrix4d& matrix) {
  nlohmann::json rows = nlohmann::json::array();
  for (int row = 0; row < 4; ++row) {
    nlohmann::json cols = nlohmann::json::array();
    for (int col = 0; col < 4; ++col) {
      cols.push_back(matrix(row, col));
    }
    rows.push_back(cols);
  }
  return rows;
}

}  // namespace

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

void WriteUndistortJson(const std::string& output_path,
                             const CalibrationParam& calibration,
                             const NewIntrinsicConfig& new_intrinsic) {
  Eigen::Matrix3d intrinsics = Eigen::Matrix3d::Identity();
  intrinsics(0, 0) = new_intrinsic.focal_u;
  intrinsics(0, 2) =
      new_intrinsic.center == 0 ? calibration.intrinsic_matrix(0, 2) : new_intrinsic.center_u;
  intrinsics(1, 1) = new_intrinsic.focal_v;
  intrinsics(1, 2) =
      new_intrinsic.center == 0 ? calibration.intrinsic_matrix(1, 2) : new_intrinsic.center_v;

  const std::vector<double> zero_dist(calibration.dist_data.size(), 0.0);
  nlohmann::json json = {
      {"undistort_setting",
       {{"intrinsics", Matrix3dToJson(intrinsics)},
        {"extrinsics", Matrix4dToJson(calibration.extrinsic_matrix)},
        {"distort", zero_dist}}}};
  WriteJson(output_path, json);
}

void WriteVirtualJson(const std::string& output_path,
                           const Eigen::Matrix3d& virtual_intrinsic,
                           const Eigen::Matrix4d& virtual_extrinsic,
                           const std::vector<double>& dist_data) {
  nlohmann::json json = {
      {"virtual_camera_setting",
       {{"intrinsics", Matrix3dToJson(virtual_intrinsic)},
        {"extrinsics", Matrix4dToJson(virtual_extrinsic)},
        {"distort", dist_data}}}};
  WriteJson(output_path, json);
}

}  // namespace vc
