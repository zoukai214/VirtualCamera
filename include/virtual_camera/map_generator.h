#pragma once

#include "virtual_camera/types.h"

#include <Eigen/Dense>
#include <opencv2/core.hpp>

#include <string>
#include <vector>

namespace vc {

class MapGenerator {
 public:
  MapGenerator(const CalibrationParam& calibration, const VirtualParam& virtual_param);

  std::vector<cv::Mat> CreateVirtualMap(bool fisheye_model = true) const;
  std::vector<cv::Mat> CreateUndistortMap() const;
  std::vector<cv::Mat> CreateResizeMap(Eigen::Matrix3d* resized_intrinsic) const;
  void SaveBin(const std::string& save_path, const std::vector<cv::Mat>& maps,
               int source_width, int source_height, int output_width, int output_height) const;

  const Eigen::Matrix3d& virtual_intrinsic() const { return virtual_intrinsic_; }
  const Eigen::Matrix4d& virtual_extrinsic() const { return virtual_extrinsic_; }

 private:
  Eigen::Matrix4d BuildVirtualExtrinsic() const;
  Eigen::Matrix3d BuildVirtualIntrinsic() const;
  cv::Mat BuildCameraMatrix() const;
  cv::Mat BuildDistortion(int count) const;

  CalibrationParam calibration_;
  VirtualParam virtual_param_;
  Eigen::Matrix3d virtual_intrinsic_;
  Eigen::Matrix4d virtual_extrinsic_;
};

}  // namespace vc
