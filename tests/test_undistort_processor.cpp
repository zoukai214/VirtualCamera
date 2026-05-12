#include "virtual_camera/pipeline_config.h"
#include "virtual_camera/undistort_processor.h"

#include <filesystem>
#include <stdexcept>
#include <string>

namespace {

void Expect(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

vc::NewIntrinsicConfig MakeNewIntrinsic() {
  vc::NewIntrinsicConfig new_intrinsic;
  new_intrinsic.focal_u = 1000.0;
  new_intrinsic.center_u = 1920.0;
  new_intrinsic.focal_v = 1000.0;
  new_intrinsic.center_v = 1080.0;
  new_intrinsic.image_width = 3840;
  new_intrinsic.image_height = 2160;
  new_intrinsic.center = 1;
  return new_intrinsic;
}

std::filesystem::path MakeTestRoot(const std::string& name) {
  const std::filesystem::path root =
      std::filesystem::path("build/test_tmp/undistort_processor") / name;
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  return root;
}

vc::PipelineConfig MakeBaseConfig(const std::filesystem::path& output_root) {
  vc::PipelineConfig config;
  config.dataset_root = "/workspace/GACRT024_1754812994";
  config.output_root = output_root.string();
  config.paths.dataset_root = config.dataset_root;
  config.paths.conf_dir_path = "calib_extract";
  config.paths.image_dir_path = "image_raw";
  config.paths.undistort_conf_dir_path = "calib_undistortion";
  config.paths.undistort_image_dir_path = "image_undistortion";
  config.distort_model = 0;
  config.process_undistort = 1;
  config.undistort_parallelism = 1;
  return config;
}

vc::UndistortTaskConfig MakeTask(const std::string& conf_json,
                                 const std::string& image_dir) {
  vc::UndistortTaskConfig task;
  task.conf_json = conf_json;
  task.intri_key = "camera-front-wide";
  task.extri_key = "camera-front-wide-to-car";
  task.image_dir = image_dir;
  task.new_intrinsic = MakeNewIntrinsic();
  return task;
}

void TestRunUndistortPipelineSerialStopsAfterFirstFailure() {
  const std::filesystem::path root = MakeTestRoot("serial_fail_fast");
  vc::PipelineConfig config = MakeBaseConfig(root);
  config.undistort_parallelism = 1;
  config.undistort_tasks.push_back(
      MakeTask("missing_calibration.json", "front_wide/"));
  config.undistort_tasks.push_back(
      MakeTask("calib_camera_front_wide_to_car.json", "front_wide/"));

  bool thrown = false;
  try {
    vc::RunUndistortPipeline(config);
  } catch (const std::runtime_error&) {
    thrown = true;
  }
  Expect(thrown, "serial mode should fail on first undistort task");
  Expect(!std::filesystem::exists(root / "calib_undistortion" /
                                  "calib_camera_front_wide_to_car.json"),
         "second task json should not exist");
  Expect(!std::filesystem::exists(root / "image_undistortion" / "front_wide"),
         "second task image dir should not exist");
}

void TestRunUndistortPipelineRejectsDuplicateJsonOutputs() {
  const std::filesystem::path root = MakeTestRoot("parallel_json_collision");
  vc::PipelineConfig config = MakeBaseConfig(root);
  config.undistort_parallelism = 2;
  config.undistort_tasks.push_back(MakeTask("cam.json", "front_wide/"));
  config.undistort_tasks.push_back(MakeTask("./cam.json", "front_narrow/"));

  bool thrown = false;
  try {
    vc::RunUndistortPipeline(config);
  } catch (const std::runtime_error& error) {
    const std::string message = error.what();
    thrown = message.find("duplicate undistort output json path") != std::string::npos;
  }
  Expect(thrown, "parallel mode should reject duplicate json outputs");
  Expect(!std::filesystem::exists(root / "calib_undistortion"),
         "duplicate json validation should happen before work starts");
  Expect(!std::filesystem::exists(root / "image_undistortion"),
         "duplicate json validation should not create image outputs");
}

void TestRunUndistortPipelineRejectsDuplicateImageOutputs() {
  const std::filesystem::path root = MakeTestRoot("parallel_image_collision");
  vc::PipelineConfig config = MakeBaseConfig(root);
  config.undistort_parallelism = 2;
  config.undistort_tasks.push_back(MakeTask("front_wide_a.json", "front_wide/"));
  config.undistort_tasks.push_back(MakeTask("front_wide_b.json", "front_wide//"));

  bool thrown = false;
  try {
    vc::RunUndistortPipeline(config);
  } catch (const std::runtime_error& error) {
    const std::string message = error.what();
    thrown = message.find("duplicate undistort output image path") != std::string::npos;
  }
  Expect(thrown, "parallel mode should reject duplicate image outputs");
  Expect(!std::filesystem::exists(root / "calib_undistortion"),
         "duplicate image validation should happen before work starts");
  Expect(!std::filesystem::exists(root / "image_undistortion"),
         "duplicate image validation should not create image outputs");
}

}  // namespace

int main() {
  TestRunUndistortPipelineSerialStopsAfterFirstFailure();
  TestRunUndistortPipelineRejectsDuplicateJsonOutputs();
  TestRunUndistortPipelineRejectsDuplicateImageOutputs();
  return 0;
}
