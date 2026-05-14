#pragma once

#include "virtual_camera/pipeline_config.h"
#include "virtual_camera/types.h"

#include <opencv2/core.hpp>

#include <string>

namespace vc {

struct UndistortMaps {
  cv::Mat map_x;
  cv::Mat map_y;
};

struct VirtualCameraMaps {
  cv::Mat map_x;
  cv::Mat map_y;
  cv::Mat src_map_x;
  cv::Mat src_map_y;
};

UndistortMaps GenerateUndistortMaps(const CalibrationParam& calibration,
                                    const NewIntrinsicConfig& new_intrinsic,
                                    int distort_model);

VirtualCameraMaps GenerateVirtualCameraMaps(const CalibrationParam& calibration,
                                            const VirtualCameraTaskConfig& task,
                                            int distort_model);

void SaveFloatMapFile(const std::string& path, const cv::Mat& map);

}  // namespace vc
