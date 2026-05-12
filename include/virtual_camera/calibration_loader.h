#pragma once

#include "virtual_camera/types.h"

#include <string>

namespace vc {

CalibrationParam LoadRt024Calibration(const std::string& calib_dir,
                                      const std::string& conf_json,
                                      const std::string& intri_key,
                                      const std::string& extri_key,
                                      int distort_model);

}  // namespace vc
