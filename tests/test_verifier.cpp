#include "virtual_camera/json_utils.h"
#include "virtual_camera/verifier.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace {

void WriteFile(const std::string& path, const std::string& data) {
  vc::EnsureDirectory(std::filesystem::path(path).parent_path().string());
  std::ofstream output(path, std::ios::binary);
  output << data;
}

}  // namespace

int main() {
  const std::filesystem::path root = "build/test_verifier_tmp";
  std::filesystem::remove_all(root);

  WriteFile((root / "golden/calib/gdc/a.bin").string(), "abc");
  WriteFile((root / "actual/calib/gdc/a.bin").string(), "abc");
  WriteFile((root / "golden/calib/gdc_intri/b.bin").string(), "def");
  WriteFile((root / "actual/calib/gdc_intri/b.bin").string(), "def");

  nlohmann::json golden_intri = {
      {"value0",
       {{"param",
         {{"cam_K", {{"data", {{1, 0, 2}, {0, 1, 3}, {0, 0, 1}}}}},
          {"cam_K_new", {{"data", {{2, 0, 2}, {0, 2, 3}, {0, 0, 1}}}}},
          {"cam_dist", {{"data", {{0.1, 0.2, 0.3, 0.4}}}}},
          {"img_new_w", 1024},
          {"img_new_h", 512}}}}}};
  nlohmann::json golden_extri = {
      {"value0",
       {{"param",
         {{"sensor_calib",
           {{"data", {{1, 0, 0, 1}, {0, 1, 0, 2}, {0, 0, 1, 3}, {0, 0, 0, 1}}}}}}}}}};

  vc::WriteJson((root / "golden/calib/virtual/cam/cam-intrinsic.json").string(), golden_intri);
  vc::WriteJson((root / "actual/calib/virtual/cam/cam-intrinsic.json").string(), golden_intri);
  vc::WriteJson((root / "golden/calib/virtual/cam/cam-to-car_center-extrinsic.json").string(), golden_extri);
  vc::WriteJson((root / "actual/calib/virtual/cam/cam-to-car_center-extrinsic.json").string(), golden_extri);

  const auto result =
      vc::VerifyGdcOutputs((root / "golden").string(), (root / "actual").string());
  if (!result.ok) {
    std::cerr << result.message << "\n";
    return 1;
  }

  WriteFile((root / "actual/calib/gdc/a.bin").string(), "abx");
  const auto failed =
      vc::VerifyGdcOutputs((root / "golden").string(), (root / "actual").string());
  if (failed.ok || failed.message.find("byte mismatch") == std::string::npos) {
    std::cerr << "expected byte mismatch failure\n";
    return 1;
  }

  return 0;
}
