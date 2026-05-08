#pragma once

#include "virtual_camera/types.h"

#include <Eigen/Dense>

#include <string>
#include <vector>

namespace vc {

void SaveVirtualJson(const std::string& input_root, const std::string& output_root,
                     const CameraTask& task, const Eigen::Matrix4d& virtual_extrinsic,
                     const Eigen::Matrix3d& virtual_intrinsic,
                     const std::vector<double>& dist_data);

}  // namespace vc
