#ifndef CAMERA_MAPS_GENERATOR_H
#define CAMERA_MAPS_GENERATOR_H

#include "data_types.h"
#include <vector>

class CameraMapsGenerator {
public:
    // 生成相机映射表（优化版本）
    static CameraMaps generateCameraMapsOptimized(
        const ResultSizeParam& result_param,
        const JuncView4ConfigCommon& config_2D,
        const std::vector<CameraModelExt>& cam_model_ext,
        const std::vector<CameraModelInt>& cam_model_int,
        int image_width,
        int image_height,
        double vehicle_width,
        double vehicle_length,
        double vehicle_overhang
    );
};

#endif // CAMERA_MAPS_GENERATOR_H

