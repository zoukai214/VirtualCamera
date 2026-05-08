#include "virtual_camera/json_utils.h"

#include <filesystem>
#include <fstream>
#include <iostream>

int main() {
  const std::filesystem::path path = "build/test_json_utils.json";
  std::ofstream out(path);
  out << R"({"matrix":[[1,2,3],[4,5,6],[7,8,9]],"dist":[[0.1,0.2,0.3,0.4]]})";
  out.close();

  const auto json = vc::ReadJson(path.string());
  const auto matrix = vc::JsonToMatrix3d(json.at("matrix"));
  const auto dist = vc::JsonToVector(json.at("dist"));

  if (matrix(0, 0) != 1.0 || matrix(2, 2) != 9.0) {
    std::cerr << "matrix conversion failed\n";
    return 1;
  }
  if (dist.size() != 4 || dist[2] != 0.3) {
    std::cerr << "vector conversion failed\n";
    return 1;
  }
  return 0;
}
