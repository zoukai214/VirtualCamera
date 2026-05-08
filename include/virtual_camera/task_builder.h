#pragma once

#include "virtual_camera/types.h"

#include <nlohmann_json/json.hpp>

#include <string>
#include <vector>

namespace vc {

std::vector<CameraTask> BuildThorTasks(const nlohmann::json& config);
CalibrationParam LoadCalibration(const std::string& input_root, const CameraTask& task);

}  // namespace vc
