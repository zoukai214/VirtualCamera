#include "virtual_camera/calibration_loader.h"
#include "virtual_camera/remap_generator.h"

#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Expect(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

std::vector<char> ReadBytes(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input.is_open()) {
    throw std::runtime_error("failed to open file: " + path.string());
  }
  return std::vector<char>(std::istreambuf_iterator<char>(input),
                           std::istreambuf_iterator<char>());
}

void TestGenerateUndistortMapsReturnsFloatMaps() {
  vc::CalibrationParam calibration;
  calibration.intrinsic_matrix << 1000.0, 0.0, 960.0, 0.0, 1000.0, 540.0, 0.0, 0.0, 1.0;
  calibration.dist_data = {0.01, -0.02, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
  calibration.image_width = 1920;
  calibration.image_height = 1080;

  vc::NewIntrinsicConfig intrinsic;
  intrinsic.focal_u = 900.0;
  intrinsic.focal_v = 900.0;
  intrinsic.center_u = 960.0;
  intrinsic.center_v = 540.0;
  intrinsic.image_width = 1920;
  intrinsic.image_height = 1080;
  intrinsic.center = 1;

  const vc::UndistortMaps maps = vc::GenerateUndistortMaps(calibration, intrinsic, 0);

  Expect(maps.map_x.type() == CV_32FC1, "map_x should be CV_32FC1");
  Expect(maps.map_y.type() == CV_32FC1, "map_y should be CV_32FC1");
  Expect(maps.map_x.rows == 1080 && maps.map_x.cols == 1920,
         "map_x size should match output");
}

void TestSaveFloatMapFileWritesExpectedBytes() {
  const std::filesystem::path root = std::filesystem::path("build/test_tmp/remap_generator");
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);

  cv::Mat map(2, 2, CV_32FC1);
  map.at<float>(0, 0) = 1.0f;
  map.at<float>(0, 1) = 2.0f;
  map.at<float>(1, 0) = 3.0f;
  map.at<float>(1, 1) = 4.0f;

  const std::filesystem::path output = root / "map.bin";
  vc::SaveFloatMapFile(output.string(), map);
  Expect(std::filesystem::file_size(output) == 4 * sizeof(float),
         "saved map should contain raw float payload");

  std::ifstream input(output, std::ios::binary);
  float values[4] = {};
  input.read(reinterpret_cast<char*>(values), sizeof(values));
  Expect(values[0] == 1.0f && values[3] == 4.0f, "saved bytes should keep float order");
}

void TestGenerateVirtualCameraMapsKeepsSourceSizedSrcMaps() {
  vc::CalibrationParam calibration;
  calibration.intrinsic_matrix << 800.0, 0.0, 320.0, 0.0, 810.0, 240.0, 0.0, 0.0, 1.0;
  calibration.extrinsic_matrix = Eigen::Matrix4d::Identity();
  calibration.dist_data = {0.01, -0.02, 0.001, -0.001, 0.0, 0.0, 0.0, 0.0};
  calibration.image_width = 640;
  calibration.image_height = 480;

  vc::VirtualCameraTaskConfig task;
  task.camera_id = 2;
  task.image_width = 640;
  task.image_height = 480;
  task.fov = 100;
  task.new_intrinsic.fov = 90.0;
  task.new_intrinsic.focal_u = 256.0;
  task.new_intrinsic.focal_v = 256.0;
  task.new_intrinsic.center_u = 256.0;
  task.new_intrinsic.center_v = 128.0;
  task.new_intrinsic.image_width = 512;
  task.new_intrinsic.image_height = 256;
  task.new_extrinsics.yaw = 15.0;

  const vc::VirtualCameraMaps maps = vc::GenerateVirtualCameraMaps(calibration, task, 0);

  Expect(maps.map_x.rows == 256 && maps.map_x.cols == 512,
         "virtual map should use destination size");
  Expect(maps.src_map_x.rows == 480 && maps.src_map_x.cols == 640,
         "src map should keep source size");
  Expect(maps.src_map_x.type() == CV_32FC1 && maps.src_map_y.type() == CV_32FC1,
         "src maps should be float");
}

void TestGenerateVirtualCameraMapsMatchesFw110GoldenSrcMaps() {
  const std::filesystem::path root = std::filesystem::path("build/test_tmp/remap_generator");
  std::filesystem::remove_all(root / "fw110_exact");
  std::filesystem::create_directories(root / "fw110_exact");

  const vc::CalibrationParam calibration = vc::LoadCalibration(
      "/workspace/GACRT024_1754812994/calib_extract",
      "calib_camera_front_wide_to_car.json",
      "camera-front-wide",
      "camera-front-wide-to-car",
      0);

  vc::VirtualCameraTaskConfig task;
  task.desc = "front fov120->front fov 110";
  task.conf_json = "calib_camera_front_wide_to_car.json";
  task.conf_intri_key = "camera-front-wide";
  task.conf_extri_key = "camera-front-wide-to-car";
  task.image_dir = "front_wide/";
  task.save_dir = "front_wide_110/";
  task.calib_json = "calib_cam_front_wide_fov110.json";
  task.file_prefix = "fw110";
  task.camera_id = 1;
  task.image_width = 3840;
  task.image_height = 2160;
  task.fov = 120;
  task.undistort_image = 1;
  task.vc_mapx_name = "fw110_vc_mapX.bin";
  task.vc_mapy_name = "fw110_vc_mapY.bin";
  task.src2vc_mapx_name = "fw110_src2vc_mapX.bin";
  task.src2vc_mapy_name = "fw110_src2vc_mapY.bin";
  task.new_intrinsic.fov = 110.0;
  task.new_intrinsic.focal_u = 358.5;
  task.new_intrinsic.focal_v = 358.5;
  task.new_intrinsic.center_u = 512.0;
  task.new_intrinsic.center_v = 256.0;
  task.new_intrinsic.image_width = 1024;
  task.new_intrinsic.image_height = 512;
  task.new_extrinsics.pitch = 0.0;
  task.new_extrinsics.roll = 0.0;
  task.new_extrinsics.yaw = 0.0;
  task.new_extrinsics.x = 1.954432;
  task.new_extrinsics.y = -0.049998;
  task.new_extrinsics.z = 1.54734;

  const vc::VirtualCameraMaps maps = vc::GenerateVirtualCameraMaps(calibration, task, 0);
  const std::filesystem::path actual_x = root / "fw110_exact/fw110_src2vc_mapX.bin";
  const std::filesystem::path actual_y = root / "fw110_exact/fw110_src2vc_mapY.bin";
  vc::SaveFloatMapFile(actual_x.string(), maps.src_map_x);
  vc::SaveFloatMapFile(actual_y.string(), maps.src_map_y);

  const std::filesystem::path golden_x =
      "/workspace/GACRT024_1754812994/vc_gdcbin_dir_path/fw110_src2vc_mapX.bin";
  const std::filesystem::path golden_y =
      "/workspace/GACRT024_1754812994/vc_gdcbin_dir_path/fw110_src2vc_mapY.bin";

  Expect(ReadBytes(actual_x) == ReadBytes(golden_x), "fw110 src2vc mapX should match golden");
  Expect(ReadBytes(actual_y) == ReadBytes(golden_y), "fw110 src2vc mapY should match golden");
}

}  // namespace

int main() {
  TestGenerateUndistortMapsReturnsFloatMaps();
  TestSaveFloatMapFileWritesExpectedBytes();
  TestGenerateVirtualCameraMapsKeepsSourceSizedSrcMaps();
  TestGenerateVirtualCameraMapsMatchesFw110GoldenSrcMaps();
  return 0;
}
