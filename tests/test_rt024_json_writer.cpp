#include "virtual_camera/json_utils.h"
#include "virtual_camera/json_writer.h"
#include "virtual_camera/pipeline_config.h"

#include <Eigen/Dense>

#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Expect(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void TestWriteUndistortJson() {
  const std::string output = "build/test_tmp/rt024_json/undistort.json";
  vc::CalibrationParam calibration;
  calibration.extrinsic_matrix = Eigen::Matrix4d::Identity();
  calibration.intrinsic_matrix = Eigen::Matrix3d::Identity();
  calibration.intrinsic_matrix(0, 2) = 321.0;
  calibration.intrinsic_matrix(1, 2) = 123.0;
  calibration.dist_data = {9, 9, 9, 9, 9, 9, 9, 9};

  vc::NewIntrinsicConfig new_intrinsic;
  new_intrinsic.focal_u = 100.0;
  new_intrinsic.center_u = 50.0;
  new_intrinsic.focal_v = 120.0;
  new_intrinsic.center_v = 20.0;
  new_intrinsic.image_width = 128;
  new_intrinsic.image_height = 64;
  new_intrinsic.center = 0;

  vc::WriteUndistortJson(output, calibration, new_intrinsic);
  const auto json = vc::ReadJson(output);
  Expect(json.contains("undistort_setting"), "undistort_setting");
  const auto& setting = json.at("undistort_setting");
  Expect(setting.at("intrinsics").at(0).at(0).get<double>() == 100.0, "undistort fx");
  Expect(setting.at("intrinsics").at(0).at(2).get<double>() == 321.0, "undistort cx");
  Expect(setting.at("intrinsics").at(1).at(2).get<double>() == 123.0, "undistort cy");
  Expect(setting.at("distort").at(0).get<double>() == 0.0, "undistort distortion should be zero");
}

void TestWriteVirtualJson() {
  const std::string output = "build/test_tmp/rt024_json/virtual.json";
  Eigen::Matrix3d intrinsic = Eigen::Matrix3d::Identity();
  intrinsic(0, 0) = 358.5;
  intrinsic(0, 2) = 512.0;
  intrinsic(1, 1) = 358.5;
  intrinsic(1, 2) = 256.0;
  Eigen::Matrix4d extrinsic = Eigen::Matrix4d::Identity();
  const std::vector<double> dist = {0, 0, 0, 0, 0, 0, 0, 0};

  vc::WriteVirtualJson(output, intrinsic, extrinsic, dist);
  const auto json = vc::ReadJson(output);
  Expect(json.contains("virtual_camera_setting"), "virtual_camera_setting");
  const auto& setting = json.at("virtual_camera_setting");
  Expect(setting.at("intrinsics").at(0).at(0).get<double>() == 358.5, "virtual fx");
  Expect(setting.at("extrinsics").at(3).at(3).get<double>() == 1.0, "virtual homogeneous");
}

}  // namespace

int main() {
  TestWriteUndistortJson();
  TestWriteVirtualJson();
  return 0;
}
