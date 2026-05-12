#pragma once

#include "virtual_camera/pipeline_config.h"
#include "virtual_camera/types.h"

#include <Eigen/Dense>

#include <string>
#include <vector>

namespace vc {

void SaveVirtualJson(const std::string& input_root, const std::string& output_root,
                     const CameraTask& task, const Eigen::Matrix4d& virtual_extrinsic,
                     const Eigen::Matrix3d& virtual_intrinsic,
                     const std::vector<double>& dist_data);

void WriteRt024UndistortJson(const std::string& output_path,
                             const CalibrationParam& calibration,
                             const NewIntrinsicConfig& new_intrinsic);

void WriteRt024VirtualJson(const std::string& output_path,
                           const Eigen::Matrix3d& virtual_intrinsic,
                           const Eigen::Matrix4d& virtual_extrinsic,
                           const std::vector<double>& dist_data);

}  // namespace vc
