#include "virtual_camera/calibration_loader.h"

#include <stdexcept>
#include <string>

namespace {

void Expect(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void TestLoadCalibrationFromDataset() {
  const vc::CalibrationParam calibration =
      vc::LoadCalibration("/workspace/GACRT024_1754812994/calib_extract",
                               "calib_camera_front_wide_to_car.json",
                               "camera-front-wide",
                               "camera-front-wide-to-car",
                               0);
  Expect(calibration.dist_data.size() == 8, "pinhole distortion size");
  Expect(calibration.intrinsic_matrix(0, 0) > 0.0, "fx");
  Expect(calibration.extrinsic_matrix(3, 3) == 1.0, "homogeneous matrix");
}

void TestLoadCalibrationMissingKey() {
  bool thrown = false;
  try {
    static_cast<void>(vc::LoadCalibration(
        "/workspace/GACRT024_1754812994/calib_extract",
        "calib_camera_front_wide_to_car.json",
        "missing-key", "camera-front-wide-to-car", 0));
  } catch (const std::runtime_error& error) {
    thrown = std::string(error.what()).find("missing-key") != std::string::npos;
  }
  Expect(thrown, "missing intri key should be reported");
}

}  // namespace

int main() {
  TestLoadCalibrationFromDataset();
  TestLoadCalibrationMissingKey();
  return 0;
}
