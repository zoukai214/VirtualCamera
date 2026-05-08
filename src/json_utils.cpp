#include "virtual_camera/json_utils.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace vc {

nlohmann::json ReadJson(const std::string& file_path) {
  std::ifstream input(file_path);
  if (!input.is_open()) {
    throw std::runtime_error("failed to open json file: " + file_path);
  }
  nlohmann::json json;
  input >> json;
  return json;
}

void WriteJson(const std::string& file_path, const nlohmann::json& json) {
  const auto parent = std::filesystem::path(file_path).parent_path();
  if (!parent.empty()) {
    EnsureDirectory(parent.string());
  }
  std::ofstream output(file_path, std::ios::binary);
  if (!output.is_open()) {
    throw std::runtime_error("failed to write json file: " + file_path);
  }
  output << json.dump(4) << '\n';
}

bool FileExists(const std::string& file_path) {
  return std::filesystem::is_regular_file(file_path);
}

void EnsureDirectory(const std::string& dir_path) {
  std::filesystem::create_directories(dir_path);
  if (!std::filesystem::is_directory(dir_path)) {
    throw std::runtime_error("failed to create directory: " + dir_path);
  }
}

Eigen::Matrix4d JsonToMatrix4d(const nlohmann::json& data) {
  Eigen::Matrix4d matrix = Eigen::Matrix4d::Identity();
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col) {
      matrix(row, col) = data.at(row).at(col).get<double>();
    }
  }
  return matrix;
}

Eigen::Matrix3d JsonToMatrix3d(const nlohmann::json& data) {
  Eigen::Matrix3d matrix = Eigen::Matrix3d::Identity();
  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 3; ++col) {
      matrix(row, col) = data.at(row).at(col).get<double>();
    }
  }
  return matrix;
}

std::vector<double> JsonToVector(const nlohmann::json& data) {
  const nlohmann::json* row = &data;
  if (data.is_array() && !data.empty() && data.at(0).is_array()) {
    row = &data.at(0);
  }
  std::vector<double> values;
  values.reserve(row->size());
  for (const auto& value : *row) {
    values.push_back(value.get<double>());
  }
  return values;
}

}  // namespace vc
