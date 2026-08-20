#include "virtual_camera/calibration_loader.h"

#include "virtual_camera/json_utils.h"

#include <filesystem>
#include <stdexcept>
#include <string>

namespace vc {
namespace {

const nlohmann::json& RequiredNode(const nlohmann::json& json, const std::string& key) {
  if (!json.contains(key)) {
    throw std::runtime_error("missing calibration key: " + key);
  }
  return json.at(key);
}

}  // namespace

CalibrationParam LoadCalibration(const std::string& calib_dir,
                                      const std::string& conf_json,
                                      const std::string& intri_key,
                                      const std::string& extri_key,
                                      int distort_model) {
  const std::filesystem::path path = std::filesystem::path(calib_dir) / conf_json;
  const nlohmann::json root = ReadJson(path.string());

  const auto& intri = RequiredNode(root, intri_key);
  const auto& extri = RequiredNode(root, extri_key);

  CalibrationParam calibration;
  calibration.intrinsic_matrix =
      JsonToMatrix3d(intri.at("param").at("cam_matrix").at("data"));
  calibration.extrinsic_matrix =
      JsonToMatrix4d(extri.at("param").at("sensor_calib").at("data"));
  calibration.dist_data = JsonToVector(intri.at("param").at("cam_dist").at("data"));
  calibration.image_width = intri.at("param").at("width").get<int>();
  calibration.image_height = intri.at("param").at("height").get<int>();

  const std::size_t expected_size = distort_model == 1 ? 4U : 8U;
  if (calibration.dist_data.size() < expected_size) {
    throw std::runtime_error("distortion coeffs size too small in " + path.string());
  }
  calibration.dist_data.resize(expected_size);
  return calibration;
}

}  // namespace vc
