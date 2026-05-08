#include "virtual_camera/map_generator.h"

#include "virtual_camera/json_utils.h"

#include <opencv2/calib3d.hpp>
#include <opencv2/core/eigen.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace vc {
namespace {

constexpr double kPi = 3.14159265358979323846;

}  // namespace

MapGenerator::MapGenerator(const CalibrationParam& calibration,
                           const VirtualParam& virtual_param)
    : calibration_(calibration),
      virtual_param_(virtual_param),
      virtual_intrinsic_(BuildVirtualIntrinsic()),
      virtual_extrinsic_(BuildVirtualExtrinsic()) {}

Eigen::Matrix4d MapGenerator::BuildVirtualExtrinsic() const {
  Eigen::Matrix4d extrinsic = Eigen::Matrix4d::Identity();
  double yaw = virtual_param_.virtual_yaw;
  double pitch = 0.0;
  double roll = virtual_param_.virtual_roll;

  // VCS 坐标系转换到 OpenCV 相机坐标系。
  yaw -= 90.0;
  roll = -roll;
  roll -= 90.0;

  yaw *= kPi / 180.0;
  pitch *= kPi / 180.0;
  roll *= kPi / 180.0;

  const double sin_yaw = std::sin(yaw);
  const double cos_yaw = std::cos(yaw);
  const double sin_pitch = std::sin(pitch);
  const double cos_pitch = std::cos(pitch);
  const double sin_roll = std::sin(roll);
  const double cos_roll = std::cos(roll);

  extrinsic(0, 0) = cos_roll * cos_pitch;
  extrinsic(0, 1) = cos_roll * sin_pitch * sin_yaw - sin_roll * cos_yaw;
  extrinsic(0, 2) = cos_roll * sin_pitch * cos_yaw + sin_roll * sin_yaw;
  extrinsic(1, 0) = sin_roll * cos_pitch;
  extrinsic(1, 1) = sin_roll * sin_pitch * sin_yaw + cos_roll * cos_yaw;
  extrinsic(1, 2) = sin_roll * sin_pitch * cos_yaw - cos_roll * sin_yaw;
  extrinsic(2, 0) = -sin_pitch;
  extrinsic(2, 1) = cos_pitch * sin_yaw;
  extrinsic(2, 2) = cos_pitch * cos_yaw;
  extrinsic(0, 3) = virtual_param_.tx;
  extrinsic(1, 3) = virtual_param_.ty;
  extrinsic(2, 3) = virtual_param_.tz;
  return extrinsic;
}

Eigen::Matrix3d MapGenerator::BuildVirtualIntrinsic() const {
  Eigen::Matrix3d intrinsic = Eigen::Matrix3d::Identity();
  intrinsic(0, 0) = virtual_param_.virtual_width / 2.0 /
                    std::tan(virtual_param_.virtual_fov * (kPi / 360.0));
  intrinsic(1, 1) = intrinsic(0, 0);
  intrinsic(0, 2) = virtual_param_.virtual_width / 2.0;
  intrinsic(1, 2) = virtual_param_.virtual_height / 2.0;
  return intrinsic;
}

cv::Mat MapGenerator::BuildCameraMatrix() const {
  cv::Mat camera_matrix = cv::Mat::zeros(3, 3, CV_64FC1);
  cv::eigen2cv(calibration_.intrinsic_matrix, camera_matrix);

  // 前视 8M 标定在 2M 输出链路中使用半尺度内参。
  if ((virtual_param_.camera_id == 0 || virtual_param_.camera_id == 1) &&
      calibration_.intrinsic_matrix(0, 2) > calibration_.image_width * 0.75) {
    camera_matrix.at<double>(0, 0) *= 0.5;
    camera_matrix.at<double>(0, 2) *= 0.5;
    camera_matrix.at<double>(1, 1) *= 0.5;
    camera_matrix.at<double>(1, 2) *= 0.5;
  }
  return camera_matrix;
}

cv::Mat MapGenerator::BuildDistortion(int count) const {
  const int actual_count = std::min<int>(count, calibration_.dist_data.size());
  std::vector<double> dist(calibration_.dist_data.begin(),
                           calibration_.dist_data.begin() + actual_count);
  cv::Mat distortion(dist, true);
  distortion.convertTo(distortion, CV_32F);
  return distortion.reshape(0, actual_count);
}

std::vector<cv::Mat> MapGenerator::CreateVirtualMap(bool fisheye_model) const {
  const Eigen::Matrix3d rotation =
      virtual_extrinsic_.block<3, 3>(0, 0).inverse() *
      calibration_.extrinsic_matrix.block<3, 3>(0, 0);

  cv::Mat camera_matrix = BuildCameraMatrix();
  cv::Mat distortion = BuildDistortion(fisheye_model ? 4 : 8);
  cv::Mat rotation_new = cv::Mat::zeros(3, 3, CV_64FC1);
  cv::Mat projection = cv::Mat::zeros(3, 3, CV_64FC1);
  cv::eigen2cv(rotation, rotation_new);
  cv::eigen2cv(virtual_intrinsic_, projection);

  cv::Mat map_x;
  cv::Mat map_y;
  if (fisheye_model) {
    cv::fisheye::initUndistortRectifyMap(
        camera_matrix, distortion, rotation_new, projection,
        cv::Size(virtual_param_.virtual_width, virtual_param_.virtual_height),
        CV_32FC1, map_x, map_y);
  } else {
    cv::initUndistortRectifyMap(
        camera_matrix, distortion, rotation_new, projection,
        cv::Size(virtual_param_.virtual_width, virtual_param_.virtual_height),
        CV_32FC1, map_x, map_y);
  }
  return {map_x, map_y};
}

std::vector<cv::Mat> MapGenerator::CreateUndistortMap() const {
  cv::Mat camera_matrix = BuildCameraMatrix();
  cv::Mat distortion = BuildDistortion(4);
  cv::Mat rotation_new = cv::Mat::eye(3, 3, CV_64FC1);
  cv::Mat projection = cv::Mat::zeros(3, 3, CV_64FC1);
  projection.at<double>(0, 0) =
      std::round(calibration_.image_width / 2.0 /
                 std::tan(calibration_.fov * kPi / 360.0) * 10.0) /
      10.0;
  projection.at<double>(1, 1) = projection.at<double>(0, 0);
  projection.at<double>(0, 2) = camera_matrix.at<double>(0, 2);
  projection.at<double>(1, 2) = camera_matrix.at<double>(1, 2);
  projection.at<double>(2, 2) = 1.0;

  cv::Mat map_x;
  cv::Mat map_y;
  cv::fisheye::initUndistortRectifyMap(
      camera_matrix, distortion, rotation_new, projection,
      cv::Size(calibration_.image_width, calibration_.image_height),
      CV_32FC1, map_x, map_y);
  return {map_x, map_y};
}

std::vector<cv::Mat> MapGenerator::CreateResizeMap(Eigen::Matrix3d* resized_intrinsic) const {
  cv::Mat camera_matrix = BuildCameraMatrix();
  cv::Mat distortion = BuildDistortion(4);
  cv::Mat rotation_new = cv::Mat::eye(3, 3, CV_64FC1);

  Eigen::Matrix3d intrinsic = Eigen::Matrix3d::Identity();
  intrinsic(0, 2) = camera_matrix.at<double>(0, 2) *
                    (static_cast<double>(virtual_param_.virtual_width) / calibration_.image_width);
  intrinsic(1, 2) = camera_matrix.at<double>(1, 2) *
                    (static_cast<double>(virtual_param_.virtual_height) / calibration_.image_height);
  intrinsic(0, 0) = camera_matrix.at<double>(0, 0) *
                    (static_cast<double>(virtual_param_.virtual_width) / calibration_.image_width) *
                    (std::tan(calibration_.fov * kPi / 360.0) /
                     std::tan(virtual_param_.virtual_fov * kPi / 360.0));
  intrinsic(1, 1) = camera_matrix.at<double>(1, 1) *
                    (static_cast<double>(virtual_param_.virtual_height) / calibration_.image_height) *
                    (std::tan(calibration_.fov * kPi / 360.0) /
                     std::tan(virtual_param_.virtual_fov * kPi / 360.0));
  if (resized_intrinsic != nullptr) {
    *resized_intrinsic = intrinsic;
  }

  cv::Mat projection = cv::Mat::zeros(3, 3, CV_64FC1);
  cv::eigen2cv(intrinsic, projection);

  cv::Mat map_x;
  cv::Mat map_y;
  cv::fisheye::initUndistortRectifyMap(
      camera_matrix, distortion, rotation_new, projection,
      cv::Size(virtual_param_.virtual_width, virtual_param_.virtual_height),
      CV_32FC1, map_x, map_y);
  return {map_x, map_y};
}

void MapGenerator::SaveBin(const std::string& save_path, const std::vector<cv::Mat>& maps,
                           int source_width, int source_height,
                           int output_width, int output_height) const {
  EnsureDirectory(std::filesystem::path(save_path).parent_path().string());
  cv::Mat merged;
  cv::merge(maps, merged);
  for (int row = 0; row < output_height; ++row) {
    for (int col = 0; col < output_width; ++col) {
      const float x = merged.at<cv::Vec2f>(row, col)[0];
      const float y = merged.at<cv::Vec2f>(row, col)[1];
      if (x < 0 || y < 0 || x > source_width - 1 || y > source_height - 1) {
        merged.at<cv::Vec2f>(row, col) = cv::Vec2f(0.0f, 0.0f);
      }
    }
  }

  std::ofstream output(save_path, std::ios::binary);
  if (!output.is_open()) {
    throw std::runtime_error("failed to write bin: " + save_path);
  }
  output.write(reinterpret_cast<const char*>(merged.data),
               static_cast<std::streamsize>(output_width * output_height * sizeof(float) * 2));
}

}  // namespace vc
