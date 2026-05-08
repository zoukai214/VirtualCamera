#include "camera_params_loader.h"
#include "virtual_camera/json_utils.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace {

void WriteText(const std::filesystem::path& path, const std::string& text) {
  vc::EnsureDirectory(path.parent_path().string());
  std::ofstream output(path);
  output << text;
}

std::string CameraJson() {
  return R"json({
    "camera_settings": {
      "front": {
        "intrinsics": [[101.0, 0.0, 301.0], [0.0, 102.0, 302.0], [0.0, 0.0, 1.0]],
        "intrinsics_src": [[201.0, 0.0, 401.0], [0.0, 202.0, 402.0], [0.0, 0.0, 1.0]],
        "extrinsics": {"pose": [[1.0, 0.0, 0.0, 1.0], [0.0, 1.0, 0.0, 2.0], [0.0, 0.0, 1.0, 3.0], [0.0, 0.0, 0.0, 1.0]]},
        "distort": [0.1, 0.2, 0.3, 0.4]
      },
      "left": {
        "intrinsics": [[111.0, 0.0, 311.0], [0.0, 112.0, 312.0], [0.0, 0.0, 1.0]],
        "intrinsics_src": [[211.0, 0.0, 411.0], [0.0, 212.0, 412.0], [0.0, 0.0, 1.0]],
        "extrinsics": {"pose": [[1.0, 0.0, 0.0, 4.0], [0.0, 1.0, 0.0, 5.0], [0.0, 0.0, 1.0, 6.0], [0.0, 0.0, 0.0, 1.0]]},
        "distort": [0.5, 0.6, 0.7, 0.8]
      },
      "right": {
        "intrinsics": [[121.0, 0.0, 321.0], [0.0, 122.0, 322.0], [0.0, 0.0, 1.0]],
        "intrinsics_src": [[221.0, 0.0, 421.0], [0.0, 222.0, 422.0], [0.0, 0.0, 1.0]],
        "extrinsics": {"pose": [[1.0, 0.0, 0.0, 7.0], [0.0, 1.0, 0.0, 8.0], [0.0, 0.0, 1.0, 9.0], [0.0, 0.0, 0.0, 1.0]]},
        "distort": [0.9, 1.0, 1.1, 1.2]
      },
      "back": {
        "intrinsics": [[131.0, 0.0, 331.0], [0.0, 132.0, 332.0], [0.0, 0.0, 1.0]],
        "intrinsics_src": [[231.0, 0.0, 431.0], [0.0, 232.0, 432.0], [0.0, 0.0, 1.0]],
        "extrinsics": {"pose": [[1.0, 0.0, 0.0, 10.0], [0.0, 1.0, 0.0, 11.0], [0.0, 0.0, 1.0, 12.0], [0.0, 0.0, 0.0, 1.0]]},
        "distort": [1.3, 1.4, 1.5, 1.6]
      }
    }
  })json";
}

bool TestCameraOrderAndFields() {
  const std::filesystem::path root = "build/test_tmp/four_view_camera_params";
  std::filesystem::remove_all(root);
  const auto json_path = root / "fisheye_cam_param.json";
  WriteText(json_path, CameraJson());

  std::vector<CameraModelExt> ext;
  std::vector<CameraModelInt> intr;
  std::vector<CameraModelInt> intr_src;
  if (!CameraParamsLoader::loadCameraParams(json_path.string(), 4, ext, intr, intr_src)) {
    std::cerr << "camera params failed to load\n";
    return false;
  }

  if (intr.at(0).intrin.at(0) != 111.0 || intr.at(1).intrin.at(0) != 101.0 ||
      intr.at(2).intrin.at(0) != 121.0 || intr.at(3).intrin.at(0) != 131.0) {
    std::cerr << "camera order is not left, front, right, rear\n";
    return false;
  }
  if (intr_src.at(1).intrin.at(0) != 201.0 || intr_src.at(3).intrin.at(2) != 431.0) {
    std::cerr << "source intrinsics were not loaded correctly\n";
    return false;
  }
  if (intr.at(3).distortion_coeff.at(3) != 1.6 ||
      ext.at(3).translation.at<double>(2, 0) != 12.0) {
    std::cerr << "distortion or extrinsic values were not loaded correctly\n";
    return false;
  }
  return true;
}

}  // namespace

int main() {
  if (!TestCameraOrderAndFields()) {
    return 1;
  }
  return 0;
}
