#include "virtual_camera/remap_generator.h"

#include "virtual_camera/json_utils.h"
#include "virtual_camera/map_generator.h"

#include <opencv2/calib3d.hpp>
#include <opencv2/core/eigen.hpp>
#include <opencv2/imgproc.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace vc {
namespace {

cv::Mat BuildCameraMatrix(const CalibrationParam& calibration, int camera_id) {
  cv::Mat camera_matrix = cv::Mat::zeros(3, 3, CV_64FC1);
  cv::eigen2cv(calibration.intrinsic_matrix, camera_matrix);
  if ((camera_id == 0 || camera_id == 1) &&
      calibration.intrinsic_matrix(0, 2) > calibration.image_width * 0.75) {
    camera_matrix.at<double>(0, 0) *= 0.5;
    camera_matrix.at<double>(0, 2) *= 0.5;
    camera_matrix.at<double>(1, 1) *= 0.5;
    camera_matrix.at<double>(1, 2) *= 0.5;
  }
  return camera_matrix;
}

cv::Mat BuildDistortion(const std::vector<double>& distortion, int count, bool as_float = false) {
  std::vector<double> values(count, 0.0);
  for (int index = 0; index < count && index < static_cast<int>(distortion.size()); ++index) {
    values[index] = distortion[index];
  }
  cv::Mat distortion_mat = cv::Mat(values, true).reshape(0, count);
  if (as_float) {
    distortion_mat.convertTo(distortion_mat, CV_32F);
  }
  return distortion_mat;
}

VirtualParam BuildVirtualParam(const CalibrationParam& calibration,
                               const VirtualCameraTaskConfig& task) {
  VirtualParam param;
  param.virtual_width = task.new_intrinsic.image_width;
  param.virtual_height = task.new_intrinsic.image_height;
  param.virtual_fov = task.new_intrinsic.fov;
  param.virtual_fu = task.new_intrinsic.focal_u;
  param.virtual_cx = task.new_intrinsic.center_u;
  param.virtual_fv = task.new_intrinsic.focal_v;
  param.virtual_cy = task.new_intrinsic.center_v;
  param.virtual_yaw = task.new_extrinsics.yaw;
  param.virtual_pitch = task.new_extrinsics.pitch;
  param.virtual_roll = task.new_extrinsics.roll;
  param.tx = calibration.extrinsic_matrix(0, 3);
  param.ty = calibration.extrinsic_matrix(1, 3);
  param.tz = calibration.extrinsic_matrix(2, 3);
  param.image_width = task.image_width;
  param.image_height = task.image_height;
  param.fov = task.fov;
  param.camera_id = task.camera_id;
  return param;
}

void GenerateSourceToVirtualMaps(const CalibrationParam& calibration,
                                 const VirtualCameraTaskConfig& task,
                                 const cv::Mat& projection,
                                 const double fv_rotation[3][3],
                                 cv::Mat* map_x,
                                 cv::Mat* map_y) {
  const double m_x = calibration.extrinsic_matrix(0, 3);
  const double m_y = calibration.extrinsic_matrix(1, 3);
  const double m_z = calibration.extrinsic_matrix(2, 3);
  const double m_fx = calibration.intrinsic_matrix(0, 0);
  const double m_fy = calibration.intrinsic_matrix(1, 1);
  const double m_cx = calibration.intrinsic_matrix(0, 2);
  const double m_cy = calibration.intrinsic_matrix(1, 2);

  cv::Mat fv_camera = projection.clone();
  cv::Mat fv_distortion = cv::Mat::zeros(8, 1, CV_64FC1);
  cv::Mat fv_rotation_mat = cv::Mat::zeros(3, 3, CV_64FC1);
  cv::Mat fv_rvec = cv::Mat::zeros(3, 1, CV_64FC1);
  cv::Mat fv_tvec = cv::Mat::zeros(3, 1, CV_64FC1);
  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 3; ++col) {
      fv_rotation_mat.at<double>(row, col) = fv_rotation[col][row];
    }
  }
  cv::Rodrigues(fv_rotation_mat, fv_rvec);
  fv_tvec.at<double>(0) = -(fv_rotation[0][0] * m_x + fv_rotation[1][0] * m_y +
                            fv_rotation[2][0] * m_z);
  fv_tvec.at<double>(1) = -(fv_rotation[0][1] * m_x + fv_rotation[1][1] * m_y +
                            fv_rotation[2][1] * m_z);
  fv_tvec.at<double>(2) = -(fv_rotation[0][2] * m_x + fv_rotation[1][2] * m_y +
                            fv_rotation[2][2] * m_z);

  std::vector<cv::Point2f> distorted_points;
  distorted_points.reserve(task.image_width * task.image_height);
  for (int row = 0; row < task.image_height; ++row) {
    for (int col = 0; col < task.image_width; ++col) {
      distorted_points.emplace_back(static_cast<float>(col), static_cast<float>(row));
    }
  }

  cv::Mat real_camera = cv::Mat::zeros(3, 3, CV_64FC1);
  real_camera.at<double>(0, 0) = calibration.intrinsic_matrix(0, 0);
  real_camera.at<double>(0, 2) = calibration.intrinsic_matrix(0, 2);
  real_camera.at<double>(1, 1) = calibration.intrinsic_matrix(1, 1);
  real_camera.at<double>(1, 2) = calibration.intrinsic_matrix(1, 2);
  real_camera.at<double>(2, 2) = 1.0;

  cv::Mat real_distortion = cv::Mat::zeros(8, 1, CV_64FC1);
  for (int index = 0; index < 8 && index < static_cast<int>(calibration.dist_data.size());
       ++index) {
    real_distortion.at<double>(index) = calibration.dist_data[index];
  }

  std::vector<cv::Point2f> undistorted_points;
  cv::undistortPoints(distorted_points, undistorted_points, real_camera, real_distortion,
                      cv::Matx33d::eye(), real_camera);

  std::vector<cv::Point3f> world_points;
  world_points.reserve(undistorted_points.size());
  for (const auto& point : undistorted_points) {
    cv::Point3f camera_point;
    camera_point.x = static_cast<float>((point.x - m_cx) / m_fx);
    camera_point.y = static_cast<float>((point.y - m_cy) / m_fy);
    camera_point.z = 1.0f;

    cv::Point3f world_point;
    world_point.x = static_cast<float>(calibration.extrinsic_matrix(0, 0) * camera_point.x +
                                       calibration.extrinsic_matrix(0, 1) * camera_point.y +
                                       calibration.extrinsic_matrix(0, 2) * camera_point.z + m_x);
    world_point.y = static_cast<float>(calibration.extrinsic_matrix(1, 0) * camera_point.x +
                                       calibration.extrinsic_matrix(1, 1) * camera_point.y +
                                       calibration.extrinsic_matrix(1, 2) * camera_point.z + m_y);
    world_point.z = static_cast<float>(calibration.extrinsic_matrix(2, 0) * camera_point.x +
                                       calibration.extrinsic_matrix(2, 1) * camera_point.y +
                                       calibration.extrinsic_matrix(2, 2) * camera_point.z + m_z);
    world_points.push_back(world_point);
  }

  std::vector<cv::Point2f> virtual_points;
  cv::projectPoints(world_points, fv_rvec, fv_tvec, fv_camera, fv_distortion, virtual_points);

  map_x->create(task.image_height, task.image_width, CV_32FC1);
  map_y->create(task.image_height, task.image_width, CV_32FC1);
  int index = 0;
  for (int row = 0; row < task.image_height; ++row) {
    float* map_x_ptr = map_x->ptr<float>(row);
    float* map_y_ptr = map_y->ptr<float>(row);
    for (int col = 0; col < task.image_width; ++col) {
      map_x_ptr[col] = virtual_points[index].x;
      map_y_ptr[col] = virtual_points[index].y;
      ++index;
    }
  }
}

}  // namespace

UndistortMaps GenerateUndistortMaps(const CalibrationParam& calibration,
                                    const NewIntrinsicConfig& new_intrinsic,
                                    int distort_model) {
  cv::Mat camera_matrix = BuildCameraMatrix(calibration, -1);
  cv::Mat new_camera_matrix = cv::Mat::eye(3, 3, CV_64FC1);
  new_camera_matrix.at<double>(0, 0) = new_intrinsic.focal_u;
  new_camera_matrix.at<double>(1, 1) = new_intrinsic.focal_v;
  new_camera_matrix.at<double>(0, 2) =
      new_intrinsic.center == 0 ? calibration.intrinsic_matrix(0, 2)
                                : new_intrinsic.center_u;
  new_camera_matrix.at<double>(1, 2) =
      new_intrinsic.center == 0 ? calibration.intrinsic_matrix(1, 2)
                                : new_intrinsic.center_v;

  UndistortMaps maps;
  if (distort_model == 1) {
    cv::fisheye::initUndistortRectifyMap(
        camera_matrix, BuildDistortion(calibration.dist_data, 4),
        cv::Mat::eye(3, 3, CV_64FC1), new_camera_matrix,
        cv::Size(new_intrinsic.image_width, new_intrinsic.image_height), CV_32FC1, maps.map_x,
        maps.map_y);
  } else {
    cv::initUndistortRectifyMap(
        camera_matrix, BuildDistortion(calibration.dist_data, 8),
        cv::Mat::eye(3, 3, CV_64FC1), new_camera_matrix,
        cv::Size(new_intrinsic.image_width, new_intrinsic.image_height), CV_32FC1, maps.map_x,
        maps.map_y);
  }
  return maps;
}

VirtualCameraMaps GenerateVirtualCameraMaps(const CalibrationParam& calibration,
                                            const VirtualCameraTaskConfig& task,
                                            int distort_model) {
  const VirtualParam virtual_param = BuildVirtualParam(calibration, task);
  MapGenerator generator(calibration, virtual_param);
  const bool fisheye_model = distort_model == 1;
  const std::vector<cv::Mat> virtual_maps = generator.CreateVirtualMap(fisheye_model);
  cv::Mat projection = cv::Mat::zeros(3, 3, CV_64FC1);
  cv::eigen2cv(generator.virtual_intrinsic(), projection);

  VirtualCameraMaps maps;
  maps.map_x = virtual_maps.at(0);
  maps.map_y = virtual_maps.at(1);

  double fv_rotation[3][3];
  double m_vyaw = 0.0;
  double m_vpitch = 0.0;
  double m_vroll = task.new_extrinsics.yaw;
  m_vyaw -= 90.0;
  m_vroll = -m_vroll;
  m_vroll -= 90.0;

  m_vyaw *= M_PI / 180.0;
  m_vpitch *= M_PI / 180.0;
  m_vroll *= M_PI / 180.0;
  const double sin_yaw = std::sin(m_vyaw);
  const double cos_yaw = std::cos(m_vyaw);
  const double sin_pitch = std::sin(m_vpitch);
  const double cos_pitch = std::cos(m_vpitch);
  const double sin_roll = std::sin(m_vroll);
  const double cos_roll = std::cos(m_vroll);
  fv_rotation[0][0] = cos_roll * cos_pitch;
  fv_rotation[0][1] = cos_roll * sin_pitch * sin_yaw - sin_roll * cos_yaw;
  fv_rotation[0][2] = cos_roll * sin_pitch * cos_yaw + sin_roll * sin_yaw;
  fv_rotation[1][0] = sin_roll * cos_pitch;
  fv_rotation[1][1] = sin_roll * sin_pitch * sin_yaw + cos_roll * cos_yaw;
  fv_rotation[1][2] = sin_roll * sin_pitch * cos_yaw - cos_roll * sin_yaw;
  fv_rotation[2][0] = -sin_pitch;
  fv_rotation[2][1] = cos_pitch * sin_yaw;
  fv_rotation[2][2] = cos_pitch * cos_yaw;

  GenerateSourceToVirtualMaps(calibration, task, projection, fv_rotation, &maps.src_map_x,
                              &maps.src_map_y);
  return maps;
}

void SaveFloatMapFile(const std::string& path, const cv::Mat& map) {
  EnsureDirectory(std::filesystem::path(path).parent_path().string());
  cv::Mat contiguous = map.isContinuous() ? map : map.clone();
  std::ofstream output(path, std::ios::binary);
  if (!output.is_open()) {
    throw std::runtime_error("failed to write map: " + path);
  }
  output.write(reinterpret_cast<const char*>(contiguous.ptr<float>(0)),
               static_cast<std::streamsize>(contiguous.total() * sizeof(float)));
}

}  // namespace vc
