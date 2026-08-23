#include "virtual_camera/pipeline_config.h"
#include "virtual_camera/virtual_camera_processor.h"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include <filesystem>
#include <fstream>
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

std::filesystem::path MakeTestRoot(const std::string& name) {
  const std::filesystem::path root =
      std::filesystem::path("build/test_tmp/virtual_camera_processor") / name;
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  return root;
}

vc::NewIntrinsicConfig MakeNewIntrinsic(double fov, int image_width,
                                        int image_height, double focal) {
  vc::NewIntrinsicConfig new_intrinsic;
  new_intrinsic.fov = fov;
  new_intrinsic.focal_u = focal;
  new_intrinsic.center_u = 512.0;
  new_intrinsic.focal_v = focal;
  new_intrinsic.center_v = 256.0;
  new_intrinsic.image_width = image_width;
  new_intrinsic.image_height = image_height;
  new_intrinsic.center = 1;
  return new_intrinsic;
}

vc::NewIntrinsicConfig MakeSmallNewIntrinsic() {
  vc::NewIntrinsicConfig new_intrinsic;
  new_intrinsic.fov = 110.0;
  new_intrinsic.focal_u = 8.0;
  new_intrinsic.center_u = 8.0;
  new_intrinsic.focal_v = 8.0;
  new_intrinsic.center_v = 4.0;
  new_intrinsic.image_width = 16;
  new_intrinsic.image_height = 8;
  new_intrinsic.center = 1;
  return new_intrinsic;
}

vc::PipelineConfig MakeBaseConfig() {
  vc::PipelineConfig config;
  config.paths.conf_dir_path = "calib_extract";
  config.paths.image_dir_path = "image_raw";
  config.paths.vc_image_dir_path = "image_virtual_camera";
  config.paths.vc_conf_dir_path = "calib_virtual_camera";
  config.paths.vc_gdcbin_dir_path = "vc_gdcbin_dir_path";
  config.distort_model = 0;
  config.showinfo = 0;
  config.process_virtual_camera = 1;
  return config;
}

void WriteTinySyntheticImage(const std::filesystem::path& path) {
  cv::Mat image(8, 16, CV_8UC3, cv::Scalar(0, 0, 0));
  for (int y = 0; y < image.rows; ++y) {
    for (int x = 0; x < image.cols; ++x) {
      image.at<cv::Vec3b>(y, x) = cv::Vec3b(
          static_cast<unsigned char>(x * 7),
          static_cast<unsigned char>(y * 13),
          static_cast<unsigned char>((x + y) * 5));
    }
  }
  if (!cv::imwrite(path.string(), image)) {
    throw std::runtime_error("failed to write synthetic image: " + path.string());
  }
}

void WriteTinyCalibrationJson(const std::filesystem::path& path) {
  std::ofstream output(path);
  if (!output.is_open()) {
    throw std::runtime_error("failed to write calibration json: " + path.string());
  }
  output << R"json({
  "camera-front-wide": {
    "param": {
      "cam_matrix": {
        "data": [
          [1909.0533447265625, 0, 1902.6021728515625],
          [0, 1909.345458984375, 1088.849609375],
          [0, 0, 1]
        ]
      },
      "cam_dist": {
        "data": [[
          0.27403417229652405,
          -0.018113205209374428,
          -2.64449499809416e-05,
          1.5385621736641042e-05,
          0.0010280556743964553,
          0.6341700553894043,
          0,
          0
        ]]
      },
      "width": 3840,
      "height": 2160
    }
  },
  "camera-front-wide-to-car": {
    "param": {
      "sensor_calib": {
        "data": [
          [-0.010597652684796122, -0.010371027993724968, 0.9998900597245306, 1.9657248981983317],
          [-0.9999331002492209, 0.004745092656345318, -0.010548891963789553, -0.061965364385173805],
          [-0.00463516812569198, -0.9999349608219705, -0.010420621018465637, 1.5531924098970122],
          [0, 0, 0, 1]
        ]
      }
    }
  }
}
)json";
}

vc::PipelineConfig MakeTinyFixtureConfig(const std::filesystem::path& root) {
  const std::filesystem::path dataset_root = root / "dataset";
  const std::filesystem::path conf_dir = dataset_root / "calib_extract";
  const std::filesystem::path image_dir = dataset_root / "image_raw" / "front_wide";
  std::filesystem::create_directories(conf_dir);
  std::filesystem::create_directories(image_dir);

  WriteTinyCalibrationJson(conf_dir / "calib_camera_front_wide_to_car.json");
  WriteTinySyntheticImage(image_dir / "synthetic_front_wide.jpg");

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

vc::VirtualCameraTaskConfig MakeValidTask() {
  vc::VirtualCameraTaskConfig task;
  task.desc = "front wide -> front wide 110";
  task.conf_json = "calib_camera_front_wide_to_car.json";
  task.conf_intri_key = "camera-front-wide";
  task.conf_extri_key = "camera-front-wide-to-car";
  task.image_dir = "front_wide/";
  task.save_dir = "front_wide_110/";
  task.calib_json = "calib_cam_front_wide_fov110.json";
  task.file_prefix = "fw110";
  task.vc_mapx_name = "fw110_vc_mapX.bin";
  task.vc_mapy_name = "fw110_vc_mapY.bin";
  task.src2vc_mapx_name = "fw110_src2vc_mapX.bin";
  task.src2vc_mapy_name = "fw110_src2vc_mapY.bin";
  task.camera_id = 1;
  task.image_width = 3840;
  task.image_height = 2160;
  task.fov = 120;
  task.undistort_image = 1;
  task.new_intrinsic = MakeNewIntrinsic(110.0, 1024, 512, 358.5);
  task.new_extrinsics.pitch = 0.0;
  task.new_extrinsics.roll = 0.0;
  task.new_extrinsics.yaw = 0.0;
  return task;
}

vc::VirtualCameraTaskConfig MakeSmallLoggingTask() {
  vc::VirtualCameraTaskConfig task = MakeValidTask();
  task.image_width = 16;
  task.image_height = 8;
  task.new_intrinsic = MakeSmallNewIntrinsic();
  return task;
}

vc::VirtualCameraTaskConfig MakeDuplicateJsonTask(const std::string& calib_json,
                                                 const std::string& save_dir,
                                                 const std::string& map_tag) {
  vc::VirtualCameraTaskConfig task = MakeValidTask();
  task.save_dir = save_dir;
  task.calib_json = calib_json;
  task.vc_mapx_name = map_tag + "_vc_mapX.bin";
  task.vc_mapy_name = map_tag + "_vc_mapY.bin";
  task.src2vc_mapx_name = map_tag + "_src2vc_mapX.bin";
  task.src2vc_mapy_name = map_tag + "_src2vc_mapY.bin";
  task.file_prefix = "json";
  return task;
}

vc::VirtualCameraTaskConfig MakeDuplicateImageTask(const std::string& save_dir,
                                                   const std::string& calib_json,
                                                   const std::string& map_tag) {
  vc::VirtualCameraTaskConfig task = MakeValidTask();
  task.save_dir = save_dir;
  task.calib_json = calib_json;
  task.vc_mapx_name = map_tag + "_vc_mapX.bin";
  task.vc_mapy_name = map_tag + "_vc_mapY.bin";
  task.src2vc_mapx_name = map_tag + "_src2vc_mapX.bin";
  task.src2vc_mapy_name = map_tag + "_src2vc_mapY.bin";
  task.file_prefix = "image";
  return task;
}

vc::VirtualCameraTaskConfig MakeDuplicateMapTask(const std::string& map_name,
                                                const std::string& save_dir,
                                                const std::string& calib_json,
                                                const std::string& map_tag) {
  vc::VirtualCameraTaskConfig task = MakeValidTask();
  task.save_dir = save_dir;
  task.calib_json = calib_json;
  task.vc_mapx_name = map_name;
  task.vc_mapy_name = map_tag + "_vc_mapY.bin";
  task.src2vc_mapx_name = map_tag + "_src2vc_mapX.bin";
  task.src2vc_mapy_name = map_tag + "_src2vc_mapY.bin";
  task.file_prefix = "map";
  return task;
}

void TestRunVirtualCameraPipelineSerialStopsAfterFirstFailure() {
  const std::filesystem::path root = MakeTestRoot("serial_fail_fast");
  vc::PipelineConfig config = MakeBaseConfig();
  config.virtual_camera_parallelism = 1;
  config.virtual_tasks.push_back([]() {
    vc::VirtualCameraTaskConfig task;
    task.conf_json = "missing_calibration.json";
    task.conf_intri_key = "camera-front-wide";
    task.conf_extri_key = "camera-front-wide-to-car";
    task.image_dir = "front_wide/";
    task.save_dir = "missing_task/";
    task.calib_json = "missing_task.json";
    task.file_prefix = "missing";
    task.vc_mapx_name = "missing_vc_mapX.bin";
    task.vc_mapy_name = "missing_vc_mapY.bin";
    task.src2vc_mapx_name = "missing_src2vc_mapX.bin";
    task.src2vc_mapy_name = "missing_src2vc_mapY.bin";
    task.camera_id = 1;
    task.image_width = 3840;
    task.image_height = 2160;
    task.fov = 120;
    task.undistort_image = 1;
    task.new_intrinsic = MakeNewIntrinsic(110.0, 1024, 512, 358.5);
    return task;
  }());
  config.virtual_tasks.push_back(MakeValidTask());

  bool thrown = false;
  try {
    vc::RunVirtualCameraPipeline(config, "/workspace/GACRT024_1754812994", root.string());
  } catch (const std::runtime_error&) {
    thrown = true;
  }
  Expect(thrown, "serial mode should fail on first virtual task");
  Expect(!std::filesystem::exists(root / "calib_virtual_camera" /
                                  "calib_cam_front_wide_fov110.json"),
         "second task json should not exist");
  Expect(!std::filesystem::exists(root / "image_virtual_camera" /
                                  "front_wide_110"),
         "second task image dir should not exist");
  Expect(!std::filesystem::exists(root / "vc_gdcbin_dir_path" /
                                  "fw110_vc_mapX.bin"),
         "second task map should not exist");
}

void TestRunVirtualCameraPipelineRejectsDuplicateJsonOutputs() {
  const std::filesystem::path root = MakeTestRoot("parallel_json_collision");
  vc::PipelineConfig config = MakeBaseConfig();
  config.virtual_camera_parallelism = 2;
  config.virtual_tasks.push_back(
      MakeDuplicateJsonTask("cam.json", "json_collision_a/", "json_collision_a"));
  config.virtual_tasks.push_back(
      MakeDuplicateJsonTask("./cam.json", "json_collision_b/", "json_collision_b"));

  bool thrown = false;
  try {
    vc::RunVirtualCameraPipeline(config, "/workspace/GACRT024_1754812994", root.string());
  } catch (const std::runtime_error& error) {
    const std::string message = error.what();
    thrown = message.find("duplicate virtual camera output path") !=
             std::string::npos;
  }
  Expect(thrown, "parallel mode should reject duplicate json outputs");
  Expect(!std::filesystem::exists(root / "calib_virtual_camera"),
         "duplicate json validation should happen before work starts");
  Expect(!std::filesystem::exists(root / "image_virtual_camera"),
         "duplicate json validation should not create image outputs");
  Expect(!std::filesystem::exists(root / "vc_gdcbin_dir_path"),
         "duplicate json validation should not create map outputs");
}

void TestRunVirtualCameraPipelineRejectsDuplicateImageOutputs() {
  const std::filesystem::path root = MakeTestRoot("parallel_image_collision");
  vc::PipelineConfig config = MakeBaseConfig();
  config.virtual_camera_parallelism = 2;
  config.virtual_tasks.push_back(
      MakeDuplicateImageTask("front_wide/", "image_collision_a.json",
                             "image_collision_a"));
  config.virtual_tasks.push_back(
      MakeDuplicateImageTask("front_wide/", "image_collision_b.json",
                             "image_collision_b"));

  bool thrown = false;
  try {
    vc::RunVirtualCameraPipeline(config, "/workspace/GACRT024_1754812994", root.string());
  } catch (const std::runtime_error& error) {
    const std::string message = error.what();
    thrown = message.find("duplicate virtual camera output path") !=
             std::string::npos;
  }
  Expect(thrown, "parallel mode should reject duplicate image outputs");
  Expect(!std::filesystem::exists(root / "calib_virtual_camera"),
         "duplicate image validation should happen before work starts");
  Expect(!std::filesystem::exists(root / "image_virtual_camera"),
         "duplicate image validation should not create image outputs");
  Expect(!std::filesystem::exists(root / "vc_gdcbin_dir_path"),
         "duplicate image validation should not create map outputs");
}

void TestRunVirtualCameraPipelineAllowsSameSaveDirWithDifferentPrefixes() {
  const std::filesystem::path root = MakeTestRoot("parallel_image_no_collision");
  vc::PipelineConfig config = MakeBaseConfig();
  config.virtual_camera_parallelism = 2;

  vc::VirtualCameraTaskConfig left = MakeValidTask();
  left.save_dir = "shared_output/";
  left.file_prefix = "left";
  left.calib_json = "left.json";
  left.vc_mapx_name = "left_vc_mapX.bin";
  left.vc_mapy_name = "left_vc_mapY.bin";
  left.src2vc_mapx_name = "left_src2vc_mapX.bin";
  left.src2vc_mapy_name = "left_src2vc_mapY.bin";

  vc::VirtualCameraTaskConfig right = MakeValidTask();
  right.save_dir = "shared_output//";
  right.file_prefix = "right";
  right.calib_json = "right.json";
  right.vc_mapx_name = "right_vc_mapX.bin";
  right.vc_mapy_name = "right_vc_mapY.bin";
  right.src2vc_mapx_name = "right_src2vc_mapX.bin";
  right.src2vc_mapy_name = "right_src2vc_mapY.bin";

  config.virtual_tasks.push_back(left);
  config.virtual_tasks.push_back(right);

  vc::RunVirtualCameraPipeline(config, "/workspace/GACRT024_1754812994", root.string());

  Expect(std::filesystem::exists(root / "image_virtual_camera" / "shared_output" /
                                 "left_1754812994899000000_50_0.jpg"),
         "left prefix output should exist");
  Expect(std::filesystem::exists(root / "image_virtual_camera" / "shared_output" /
                                 "right_1754812994899000000_50_0.jpg"),
         "right prefix output should exist");
}

void TestRunVirtualCameraPipelineRejectsCrossTypeNormalizedAlias() {
  const std::filesystem::path root = MakeTestRoot("parallel_cross_type_alias");
  vc::PipelineConfig config = MakeBaseConfig();
  config.virtual_camera_parallelism = 2;

  vc::VirtualCameraTaskConfig map_task = MakeValidTask();
  map_task.save_dir = "map_alias/";
  map_task.calib_json = "map_alias.json";
  map_task.vc_mapx_name = "shared_alias.bin";
  map_task.vc_mapy_name = "map_alias_vc_mapY.bin";
  map_task.src2vc_mapx_name = "map_alias_src2vc_mapX.bin";
  map_task.src2vc_mapy_name = "map_alias_src2vc_mapY.bin";
  map_task.file_prefix = "mapalias";

  vc::VirtualCameraTaskConfig json_task = MakeValidTask();
  json_task.save_dir = "json_alias/";
  json_task.calib_json = "../vc_gdcbin_dir_path/shared_alias.bin";
  json_task.vc_mapx_name = "json_alias_vc_mapX.bin";
  json_task.vc_mapy_name = "json_alias_vc_mapY.bin";
  json_task.src2vc_mapx_name = "json_alias_src2vc_mapX.bin";
  json_task.src2vc_mapy_name = "json_alias_src2vc_mapY.bin";
  json_task.file_prefix = "jsonalias";

  config.virtual_tasks.push_back(map_task);
  config.virtual_tasks.push_back(json_task);

  bool thrown = false;
  try {
    vc::RunVirtualCameraPipeline(config, "/workspace/GACRT024_1754812994", root.string());
  } catch (const std::runtime_error& error) {
    const std::string message = error.what();
    thrown = message.find("duplicate virtual camera output path") !=
             std::string::npos &&
             message.find("shared_alias.bin") != std::string::npos;
  }
  Expect(thrown, "parallel mode should reject cross-type aliases");
  Expect(!std::filesystem::exists(root / "calib_virtual_camera"),
         "cross-type alias validation should happen before work starts");
  Expect(!std::filesystem::exists(root / "image_virtual_camera"),
         "cross-type alias validation should not create image outputs");
  Expect(!std::filesystem::exists(root / "vc_gdcbin_dir_path"),
         "cross-type alias validation should not create map outputs");
}

void TestRunVirtualCameraPipelineRejectsDuplicateMapOutputs() {
  const std::filesystem::path root = MakeTestRoot("parallel_map_collision");
  vc::PipelineConfig config = MakeBaseConfig();
  config.virtual_camera_parallelism = 2;
  config.virtual_tasks.push_back(MakeDuplicateMapTask(
      "fw110_vc_mapX.bin", "map_collision_a/", "map_collision_a.json",
      "map_collision_a"));
  config.virtual_tasks.push_back(MakeDuplicateMapTask(
      "./fw110_vc_mapX.bin", "map_collision_b/", "map_collision_b.json",
      "map_collision_b"));

  bool thrown = false;
  try {
    vc::RunVirtualCameraPipeline(config, "/workspace/GACRT024_1754812994", root.string());
  } catch (const std::runtime_error& error) {
    const std::string message = error.what();
    thrown = message.find("duplicate virtual camera output path") !=
             std::string::npos;
  }
  Expect(thrown, "parallel mode should reject duplicate map outputs");
  Expect(!std::filesystem::exists(root / "calib_virtual_camera"),
         "duplicate map validation should happen before work starts");
  Expect(!std::filesystem::exists(root / "image_virtual_camera"),
         "duplicate map validation should not create image outputs");
  Expect(!std::filesystem::exists(root / "vc_gdcbin_dir_path"),
         "duplicate map validation should not create map outputs");
}

void TestRunVirtualCameraPipelinePrintsTaskLogsWhenShowinfoEnabled() {
  const std::filesystem::path root = MakeTestRoot("logging_enabled");
  vc::PipelineConfig config = MakeTinyFixtureConfig(root);
  config.showinfo = 1;
  config.virtual_camera_parallelism = 2;
  config.virtual_tasks.push_back(MakeSmallLoggingTask());

  const std::string output = CaptureStdout([&config, &root]() {
    vc::RunVirtualCameraPipeline(config, (root / "dataset").string(),
                                 root.string());
  });

  const std::size_t pipeline_start =
      output.find("[INFO] virtual_camera start: tasks=1 parallelism=2\n");
  const std::size_t task_start = output.find(
      "[INFO] virtual task start: prefix=fw110 save_dir=front_wide_110/ "
      "calib=calib_cam_front_wide_fov110.json\n");
  const std::size_t task_done = output.find(
      "[INFO] virtual task done: prefix=fw110 save_dir=front_wide_110/ "
      "elapsed_ms=");
  const std::size_t pipeline_done =
      output.find("[INFO] virtual_camera done: elapsed_ms=");

  Expect(pipeline_start != std::string::npos,
         "showinfo should print virtual pipeline start log");
  Expect(task_start != std::string::npos,
         "showinfo should print virtual task start log");
  Expect(task_done != std::string::npos,
         "showinfo should print virtual task done log");
  Expect(pipeline_done != std::string::npos,
         "showinfo should print virtual pipeline done log");
  Expect(pipeline_start < task_start,
         "pipeline start log should appear before task start log");
  Expect(task_start < task_done,
         "task start log should appear before task done log");
  Expect(task_done < pipeline_done,
         "task done log should appear before pipeline done log");
}

void TestRunVirtualCameraPipelineSkipsTaskLogsWhenShowinfoDisabled() {
  const std::filesystem::path root = MakeTestRoot("logging_disabled");
  vc::PipelineConfig config = MakeTinyFixtureConfig(root);
  config.showinfo = 0;
  config.virtual_camera_parallelism = 2;
  config.virtual_tasks.push_back(MakeSmallLoggingTask());

  const std::string output = CaptureStdout([&config, &root]() {
    vc::RunVirtualCameraPipeline(config, (root / "dataset").string(),
                                 root.string());
  });

  Expect(output.empty(), "disabled showinfo should not print virtual logs");
}

void TestRunVirtualCameraPipelineWritesOutputsForRealDataset() {
  const std::filesystem::path root = MakeTestRoot("real_dataset");
  vc::PipelineConfig config = MakeBaseConfig();
  config.virtual_camera_parallelism = 1;
  config.virtual_tasks.push_back(MakeValidTask());

  vc::RunVirtualCameraPipeline(config, "/workspace/GACRT024_1754812994", root.string());

  Expect(std::filesystem::exists(root / "calib_virtual_camera" /
                                 "calib_cam_front_wide_fov110.json"),
         "virtual camera json should exist");
  Expect(std::filesystem::exists(root / "vc_gdcbin_dir_path" / "fw110_vc_mapX.bin"),
         "virtual camera map should exist");
  Expect(std::filesystem::exists(root / "image_virtual_camera" / "front_wide_110"),
         "virtual camera image dir should exist");
}

void TestBuildVirtualCameraCacheGroupsEntriesByCameraId() {
  const std::filesystem::path root = MakeTestRoot("cache_by_camera_id");
  vc::PipelineConfig config = MakeTinyFixtureConfig(root);

  vc::VirtualCameraTaskConfig front_wide = MakeSmallLoggingTask();
  front_wide.file_prefix = "wide_a";
  front_wide.calib_json = "wide_a.json";
  front_wide.vc_mapx_name = "wide_a_vc_mapX.bin";
  front_wide.vc_mapy_name = "wide_a_vc_mapY.bin";
  front_wide.src2vc_mapx_name = "wide_a_src2vc_mapX.bin";
  front_wide.src2vc_mapy_name = "wide_a_src2vc_mapY.bin";

  vc::VirtualCameraTaskConfig front_wide_second = MakeSmallLoggingTask();
  front_wide_second.file_prefix = "wide_b";
  front_wide_second.calib_json = "wide_b.json";
  front_wide_second.vc_mapx_name = "wide_b_vc_mapX.bin";
  front_wide_second.vc_mapy_name = "wide_b_vc_mapY.bin";
  front_wide_second.src2vc_mapx_name = "wide_b_src2vc_mapX.bin";
  front_wide_second.src2vc_mapy_name = "wide_b_src2vc_mapY.bin";

  config.virtual_tasks.push_back(front_wide);
  config.virtual_tasks.push_back(front_wide_second);

  const vc::VirtualCameraCache cache = vc::BuildVirtualCameraCache(config, (root / "dataset").string());

  const auto found = cache.entries_by_camera_id.find(1);
  Expect(found != cache.entries_by_camera_id.end(),
         "cache should contain front wide camera id");
  Expect(found->second.size() == 2,
         "cache should group both virtual tasks under camera id");
  const vc::VirtualCameraCacheEntry& entry = cache.entries.at(found->second.front());
  Expect(!entry.gpu_maps.map_x.empty() && !entry.gpu_maps.map_y.empty(),
         "virtual cache should upload remap maps to GPU");
  Expect(entry.gpu_maps.map_x.size() == entry.maps.map_x.size() &&
             entry.gpu_maps.map_y.size() == entry.maps.map_y.size(),
         "virtual GPU maps should match CPU map sizes");
}

void TestProcessVirtualCameraFrameUsesCameraIdCache() {
  const std::filesystem::path root = MakeTestRoot("process_frame_by_camera_id");
  vc::PipelineConfig config = MakeTinyFixtureConfig(root);
  config.virtual_tasks.push_back(MakeSmallLoggingTask());

  const vc::VirtualCameraCache cache = vc::BuildVirtualCameraCache(config, (root / "dataset").string());
  cv::Mat image(8, 16, CV_8UC3, cv::Scalar(9, 8, 7));

  const std::vector<vc::VirtualCameraFrameResult> results =
      vc::ProcessVirtualCameraFrame(cache, 1, image);
  const std::vector<vc::VirtualCameraFrameResult> missing_results =
      vc::ProcessVirtualCameraFrame(cache, 99, image);

  Expect(results.size() == 1,
         "matching camera id should produce one virtual frame");
  Expect(results.front().image.rows == 8 && results.front().image.cols == 16,
         "virtual frame should use configured virtual image size");
  Expect(results.front().task.file_prefix == "fw110",
         "virtual frame result should keep task metadata");
  Expect(missing_results.empty(),
         "unknown camera id should produce no virtual frames");
}

void TestSaveVirtualCameraFrameResultWritesExistingOutputLayout() {
  const std::filesystem::path root = MakeTestRoot("save_frame_result");
  vc::PipelineConfig config = MakeTinyFixtureConfig(root);
  vc::VirtualCameraFrameResult result;
  result.task = MakeSmallLoggingTask();
  result.image = cv::Mat(8, 16, CV_8UC3, cv::Scalar(1, 2, 3));

  vc::SaveVirtualCameraFrameResult(config, result, root.string(), "source.jpg");

  Expect(std::filesystem::exists(root / "image_virtual_camera" /
                                 "front_wide_110" / "fw110_source.jpg"),
         "virtual frame save function should keep existing output layout");
}

}  // namespace

int main() {
  TestRunVirtualCameraPipelineSerialStopsAfterFirstFailure();
  TestRunVirtualCameraPipelineRejectsDuplicateJsonOutputs();
  TestRunVirtualCameraPipelineRejectsDuplicateImageOutputs();
  TestRunVirtualCameraPipelineAllowsSameSaveDirWithDifferentPrefixes();
  TestRunVirtualCameraPipelineRejectsCrossTypeNormalizedAlias();
  TestRunVirtualCameraPipelineRejectsDuplicateMapOutputs();
  TestRunVirtualCameraPipelinePrintsTaskLogsWhenShowinfoEnabled();
  TestRunVirtualCameraPipelineSkipsTaskLogsWhenShowinfoDisabled();
  TestRunVirtualCameraPipelineWritesOutputsForRealDataset();
  TestBuildVirtualCameraCacheGroupsEntriesByCameraId();
  TestProcessVirtualCameraFrameUsesCameraIdCache();
  TestSaveVirtualCameraFrameResultWritesExistingOutputLayout();
  return 0;
}
