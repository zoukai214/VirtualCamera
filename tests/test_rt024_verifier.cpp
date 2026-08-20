#include "virtual_camera/verifier.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {

void WriteText(const std::string& path, const std::string& content) {
  const auto parent = std::filesystem::path(path).parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent);
  }
  std::ofstream output(path, std::ios::binary);
  output << content;
}

void Expect(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void TestVerifyOutputsPass() {
  const std::string golden = "build/test_tmp/rt024_verify/golden";
  const std::string actual = "build/test_tmp/rt024_verify/actual";
  WriteText(golden + "/vc_gdcbin_dir_path/a.bin", "abc");
  WriteText(actual + "/vc_gdcbin_dir_path/a.bin", "abc");
  WriteText(golden + "/calib_virtual_camera/a.json",
            "{\"virtual_camera_setting\":{\"intrinsics\":[[1,0,0],[0,1,0],[0,0,1]],"
            "\"extrinsics\":[[1,0,0,0],[0,1,0,0],[0,0,1,0],[0,0,0,1]],"
            "\"distort\":[0,0,0,0,0,0,0,0]}}");
  WriteText(actual + "/calib_virtual_camera/a.json",
            "{\"virtual_camera_setting\":{\"intrinsics\":[[1,0,0],[0,1,0],[0,0,1]],"
            "\"extrinsics\":[[1,0,0,0],[0,1,0,0],[0,0,1,0],[0,0,0,1]],"
            "\"distort\":[0,0,0,0,0,0,0,0]}}");
  WriteText(golden + "/calib_undistortion/a.json",
            "{\"undistort_setting\":{\"intrinsics\":[[1,0,0],[0,1,0],[0,0,1]],"
            "\"extrinsics\":[[1,0,0,0],[0,1,0,0],[0,0,1,0],[0,0,0,1]],"
            "\"distort\":[0,0,0,0,0,0,0,0]}}");
  WriteText(actual + "/calib_undistortion/a.json",
            "{\"undistort_setting\":{\"intrinsics\":[[1,0,0],[0,1,0],[0,0,1]],"
            "\"extrinsics\":[[1,0,0,0],[0,1,0,0],[0,0,1,0],[0,0,0,1]],"
            "\"distort\":[0,0,0,0,0,0,0,0]}}");
  const auto result = vc::VerifyOutputs(golden, actual);
  Expect(result.ok, result.message);
}

void TestVerifyOutputsFailOnBinDiff() {
  const std::string golden = "build/test_tmp/rt024_verify_diff/golden";
  const std::string actual = "build/test_tmp/rt024_verify_diff/actual";
  WriteText(golden + "/vc_gdcbin_dir_path/a.bin", "abc");
  WriteText(actual + "/vc_gdcbin_dir_path/a.bin", "abd");
  WriteText(golden + "/calib_virtual_camera/a.json",
            "{\"virtual_camera_setting\":{\"intrinsics\":[[1,0,0],[0,1,0],[0,0,1]],"
            "\"extrinsics\":[[1,0,0,0],[0,1,0,0],[0,0,1,0],[0,0,0,1]],"
            "\"distort\":[0,0,0,0,0,0,0,0]}}");
  WriteText(actual + "/calib_virtual_camera/a.json",
            "{\"virtual_camera_setting\":{\"intrinsics\":[[1,0,0],[0,1,0],[0,0,1]],"
            "\"extrinsics\":[[1,0,0,0],[0,1,0,0],[0,0,1,0],[0,0,0,1]],"
            "\"distort\":[0,0,0,0,0,0,0,0]}}");
  WriteText(golden + "/calib_undistortion/a.json",
            "{\"undistort_setting\":{\"intrinsics\":[[1,0,0],[0,1,0],[0,0,1]],"
            "\"extrinsics\":[[1,0,0,0],[0,1,0,0],[0,0,1,0],[0,0,0,1]],"
            "\"distort\":[0,0,0,0,0,0,0,0]}}");
  WriteText(actual + "/calib_undistortion/a.json",
            "{\"undistort_setting\":{\"intrinsics\":[[1,0,0],[0,1,0],[0,0,1]],"
            "\"extrinsics\":[[1,0,0,0],[0,1,0,0],[0,0,1,0],[0,0,0,1]],"
            "\"distort\":[0,0,0,0,0,0,0,0]}}");
  const auto result = vc::VerifyOutputs(golden, actual);
  Expect(!result.ok, "expected diff");
}

}  // namespace

int main() {
  TestVerifyOutputsPass();
  TestVerifyOutputsFailOnBinDiff();
  return 0;
}
