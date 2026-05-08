#ifndef CAMERA_PARAMS_LOADER_H
#define CAMERA_PARAMS_LOADER_H

#include <string>
#include <vector>
#include <map>
#include "data_types.h"

class CameraParamsLoader {
public:
    // 加载相机参数
    static bool loadCameraParams(
        const std::string& json_path,
        int camera_num,
        std::vector<CameraModelExt>& cam_model_ext,
        std::vector<CameraModelInt>& cam_model_int,
        std::vector<CameraModelInt>& cam_model_int_src
    );
};

#endif // CAMERA_PARAMS_LOADER_H

