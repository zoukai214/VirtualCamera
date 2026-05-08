#pragma once

#include <Eigen/Dense>

#include <string>
#include <unordered_map>
#include <vector>

namespace vc {

enum class TaskType {
  kVirtual,
  kUndistort,
  kResize,
};

struct CalibrationParam {
  Eigen::Matrix3d intrinsic_matrix = Eigen::Matrix3d::Identity();
  Eigen::Matrix4d extrinsic_matrix = Eigen::Matrix4d::Identity();
  std::vector<double> dist_data;
  int image_width = 0;
  int image_height = 0;
  int fov = 0;
};

struct VirtualParam {
  int virtual_width = 0;
  int virtual_height = 0;
  double virtual_fov = 0.0;
  double virtual_fu = 0.0;
  double virtual_fv = 0.0;
  double virtual_cx = 0.0;
  double virtual_cy = 0.0;
  double virtual_yaw = 0.0;
  double virtual_pitch = 0.0;
  double virtual_roll = 0.0;
  double tx = 0.0;
  double ty = 0.0;
  double tz = 0.0;
  int image_width = 0;
  int image_height = 0;
  double fov = 0.0;
  int camera_id = 0;
  std::string bin_name;
};

struct CameraTask {
  TaskType type = TaskType::kVirtual;
  std::string description;
  std::string conf_json;
  std::string intri_json;
  std::string extri_json;
  std::string bin_name;
  std::string undistort_bin_name;
  int camera_id = 0;
  int image_width = 0;
  int image_height = 0;
  int source_fov = 0;
  VirtualParam virtual_param;
};

inline const std::unordered_map<int, std::string> kCameraNames = {
    {0, "camera-front-narrow"},
    {1, "camera-front-wide"},
    {2, "camera-left-front"},
    {3, "camera-left-back"},
    {4, "camera-right-front"},
    {5, "camera-right-back"},
    {6, "camera-back"},
};

}  // namespace vc
