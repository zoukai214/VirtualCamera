#pragma once

#include <Eigen/Dense>
#include <nlohmann_json/json.hpp>

#include <string>
#include <vector>

namespace vc {

nlohmann::json ReadJson(const std::string& file_path);
void WriteJson(const std::string& file_path, const nlohmann::json& json);
bool FileExists(const std::string& file_path);
void EnsureDirectory(const std::string& dir_path);
Eigen::Matrix4d JsonToMatrix4d(const nlohmann::json& data);
Eigen::Matrix3d JsonToMatrix3d(const nlohmann::json& data);
std::vector<double> JsonToVector(const nlohmann::json& data);

}  // namespace vc
