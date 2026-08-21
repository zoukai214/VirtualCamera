#include "virtual_camera/pipeline_config.h"
#include "virtual_camera/undistort_processor.h"

#include <opencv2/core.hpp>

#include <filesystem>
#include <functional>
#include <iostream>
#include <sstream>
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

vc::NewIntrinsicConfig MakeSmallNewIntrinsic() {
  vc::NewIntrinsicConfig new_intrinsic;
  new_intrinsic.focal_u = 8.0;
  new_intrinsic.center_u = 8.0;
  new_intrinsic.focal_v = 8.0;
  new_intrinsic.center_v = 4.0;
  new_intrinsic.image_width = 16;
  new_intrinsic.image_height = 8;
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

vc::PipelineConfig MakeBaseConfig() {
  vc::PipelineConfig config;
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

std::filesystem::path FindFirstFile(const std::filesystem::path& dir) {
  for (const auto& entry : std::filesystem::directory_iterator(dir)) {
    if (entry.is_regular_file()) {
      return entry.path();
    }
  }
  throw std::runtime_error("no source file found in " + dir.string());
}

vc::PipelineConfig MakeTinyFixtureConfig(const std::filesystem::path& root) {
  const std::filesystem::path dataset_root = root / "dataset";
  const std::filesystem::path conf_dir = dataset_root / "calib_extract";
  const std::filesystem::path image_dir = dataset_root / "image_raw" / "front_wide";
  std::filesystem::create_directories(conf_dir);
  std::filesystem::create_directories(image_dir);

  const std::filesystem::path source_dataset = "/workspace/GACRT024_1754812994";
  const std::filesystem::path source_conf =
      source_dataset / "calib_extract" / "calib_camera_front_wide_to_car.json";
  const std::filesystem::path source_image =
      FindFirstFile(source_dataset / "image_raw" / "front_wide");

  std::filesystem::copy_file(
      source_conf, conf_dir / "calib_camera_front_wide_to_car.json",
      std::filesystem::copy_options::overwrite_existing);
  std::filesystem::copy_file(
      source_image, image_dir / source_image.filename(),
      std::filesystem::copy_options::overwrite_existing);

  vc::PipelineConfig config = MakeBaseConfig();
  return config;
}

std::string CaptureStdout(const std::function<void()>& fn) {
  std::ostringstream capture;
  std::streambuf* const original = std::cout.rdbuf(capture.rdbuf());
  try {
    fn();
  } catch (...) {
    std::cout.rdbuf(original);
    throw;
  }
  std::cout.rdbuf(original);
  return capture.str();
}

void TestRunUndistortPipelineSerialStopsAfterFirstFailure() {
  const std::filesystem::path root = MakeTestRoot("serial_fail_fast");
  vc::PipelineConfig config = MakeBaseConfig();
  config.undistort_parallelism = 1;
  config.undistort_tasks.push_back(
      MakeTask("missing_calibration.json", "front_wide/"));
  config.undistort_tasks.push_back(
      MakeTask("calib_camera_front_wide_to_car.json", "front_wide/"));

  bool thrown = false;
  try {
    vc::RunUndistortPipeline(config, "/workspace/GACRT024_1754812994", root.string());
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
  vc::PipelineConfig config = MakeBaseConfig();
  config.undistort_parallelism = 2;
  config.undistort_tasks.push_back(MakeTask("cam.json", "front_wide/"));
  config.undistort_tasks.push_back(MakeTask("./cam.json", "front_narrow/"));

  bool thrown = false;
  try {
    vc::RunUndistortPipeline(config, "/workspace/GACRT024_1754812994", root.string());
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
  vc::PipelineConfig config = MakeBaseConfig();
  config.undistort_parallelism = 2;
  config.undistort_tasks.push_back(MakeTask("front_wide_a.json", "front_wide/"));
  config.undistort_tasks.push_back(MakeTask("front_wide_b.json", "front_wide//"));

  bool thrown = false;
  try {
    vc::RunUndistortPipeline(config, "/workspace/GACRT024_1754812994", root.string());
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

void TestRunUndistortPipelineWritesOutputsForRealDataset() {
  const std::filesystem::path root = MakeTestRoot("real_dataset");
  vc::PipelineConfig config = MakeBaseConfig();
  config.undistort_parallelism = 1;
  config.undistort_tasks.push_back(
      MakeTask("calib_camera_front_wide_to_car.json", "front_wide/"));

  vc::RunUndistortPipeline(config, "/workspace/GACRT024_1754812994", root.string());

  Expect(std::filesystem::exists(root / "calib_undistortion" /
                                 "calib_camera_front_wide_to_car.json"),
         "undistort json should exist");
  Expect(std::filesystem::exists(root / "image_undistortion" / "front_wide"),
         "undistort image dir should exist");
}

void TestRunUndistortPipelinePrintsTaskLogsWhenShowinfoEnabled() {
  const std::filesystem::path root = MakeTestRoot("logging_enabled");
  vc::PipelineConfig config = MakeTinyFixtureConfig(root);
  config.showinfo = 1;
  config.undistort_parallelism = 2;
  vc::UndistortTaskConfig task =
      MakeTask("calib_camera_front_wide_to_car.json", "front_wide/");
  task.new_intrinsic = MakeSmallNewIntrinsic();
  config.undistort_tasks.push_back(task);

  const std::string output = CaptureStdout([&config, &root]() {
    vc::RunUndistortPipeline(config, (root / "dataset").string(),
                             root.string());
  });

  const std::size_t pipeline_start =
      output.find("[INFO] undistort start: tasks=1 parallelism=2\n");
  const std::size_t task_start = output.find(
      "[INFO] undistort task start: image_dir=front_wide/ "
      "calib=calib_camera_front_wide_to_car.json\n");
  const std::size_t task_done = output.find(
      "[INFO] undistort task done: image_dir=front_wide/ elapsed_ms=");
  const std::size_t pipeline_done =
      output.find("[INFO] undistort done: elapsed_ms=");

  Expect(pipeline_start != std::string::npos,
         "showinfo should print undistort pipeline start log");
  Expect(task_start != std::string::npos,
         "showinfo should print undistort task start log");
  Expect(task_done != std::string::npos,
         "showinfo should print undistort task done log");
  Expect(pipeline_done != std::string::npos,
         "showinfo should print undistort pipeline done log");
  Expect(pipeline_start < task_start,
         "pipeline start log should appear before task start log");
  Expect(task_start < task_done,
         "task start log should appear before task done log");
  Expect(task_done < pipeline_done,
         "task done log should appear before pipeline done log");
}

void TestRunUndistortPipelineSkipsTaskLogsWhenShowinfoDisabled() {
  const std::filesystem::path root = MakeTestRoot("logging_disabled");
  vc::PipelineConfig config = MakeTinyFixtureConfig(root);
  config.showinfo = 0;
  config.undistort_parallelism = 2;
  vc::UndistortTaskConfig task =
      MakeTask("calib_camera_front_wide_to_car.json", "front_wide/");
  task.new_intrinsic = MakeSmallNewIntrinsic();
  config.undistort_tasks.push_back(task);

  const std::string output = CaptureStdout([&config, &root]() {
    vc::RunUndistortPipeline(config, (root / "dataset").string(),
                             root.string());
  });

  Expect(output.empty(), "disabled showinfo should not print undistort logs");
}

void TestBuildUndistortCacheIndexesEntriesByCameraId() {
  const std::filesystem::path root = MakeTestRoot("cache_by_camera_id");
  vc::PipelineConfig config = MakeTinyFixtureConfig(root);
  vc::UndistortTaskConfig task =
      MakeTask("calib_camera_front_wide_to_car.json", "front_wide/");
  task.new_intrinsic = MakeSmallNewIntrinsic();
  config.undistort_tasks.push_back(task);

  const vc::UndistortCache cache = vc::BuildUndistortCache(config, (root / "dataset").string());

  const auto found = cache.entries_by_camera_id.find(1);
  Expect(found != cache.entries_by_camera_id.end(),
         "undistort cache should infer front wide camera id");
  Expect(found->second.size() == 1,
         "undistort cache should keep one entry for camera id");
  const vc::UndistortCacheEntry& entry = cache.entries.at(found->second.front());
  Expect(!entry.gpu_maps.map_x.empty() && !entry.gpu_maps.map_y.empty(),
         "undistort cache should upload GPU remap maps");
  Expect(entry.gpu_maps.map_x.size() == entry.maps.map_x.size(),
         "undistort GPU map_x size should match CPU map_x");
  Expect(entry.gpu_maps.map_y.size() == entry.maps.map_y.size(),
         "undistort GPU map_y size should match CPU map_y");
}

void TestProcessUndistortFrameUsesCameraIdCache() {
  const std::filesystem::path root = MakeTestRoot("process_frame_by_camera_id");
  vc::PipelineConfig config = MakeTinyFixtureConfig(root);
  vc::UndistortTaskConfig task =
      MakeTask("calib_camera_front_wide_to_car.json", "front_wide/");
  task.new_intrinsic = MakeSmallNewIntrinsic();
  config.undistort_tasks.push_back(task);

  const vc::UndistortCache cache = vc::BuildUndistortCache(config, (root / "dataset").string());
  cv::Mat image(8, 16, CV_8UC3, cv::Scalar(4, 5, 6));

  const std::vector<vc::UndistortFrameResult> results =
      vc::ProcessUndistortFrame(cache, 1, image);
  const std::vector<vc::UndistortFrameResult> missing_results =
      vc::ProcessUndistortFrame(cache, 99, image);

  Expect(results.size() == 1,
         "matching camera id should produce one undistort frame");
  Expect(results.front().image.rows == 8 && results.front().image.cols == 16,
         "undistort frame should use configured image size");
  Expect(results.front().task.image_dir == "front_wide/",
         "undistort frame result should keep task metadata");
  Expect(missing_results.empty(),
         "unknown camera id should produce no undistort frames");
}

void TestSaveUndistortFrameResultWritesExistingOutputLayout() {
  const std::filesystem::path root = MakeTestRoot("save_frame_result");
  vc::PipelineConfig config = MakeTinyFixtureConfig(root);
  vc::UndistortFrameResult result;
  result.task = MakeTask("calib_camera_front_wide_to_car.json", "front_wide/");
  result.image = cv::Mat(8, 16, CV_8UC3, cv::Scalar(1, 2, 3));

  vc::SaveUndistortFrameResult(config, result, root.string(), "source.jpg");

  Expect(std::filesystem::exists(root / "image_undistortion" / "front_wide" /
                                 "source.jpg"),
         "undistort frame save function should keep existing output layout");
}

}  // namespace

int main() {
  TestRunUndistortPipelineSerialStopsAfterFirstFailure();
  TestRunUndistortPipelineRejectsDuplicateJsonOutputs();
  TestRunUndistortPipelineRejectsDuplicateImageOutputs();
  TestRunUndistortPipelineWritesOutputsForRealDataset();
  TestRunUndistortPipelinePrintsTaskLogsWhenShowinfoEnabled();
  TestRunUndistortPipelineSkipsTaskLogsWhenShowinfoDisabled();
  TestBuildUndistortCacheIndexesEntriesByCameraId();
  TestProcessUndistortFrameUsesCameraIdCache();
  TestSaveUndistortFrameResultWritesExistingOutputLayout();
  return 0;
}
