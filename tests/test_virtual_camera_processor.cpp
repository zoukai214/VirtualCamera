#include "virtual_camera/pipeline_config.h"
#include "virtual_camera/virtual_camera_processor.h"

#include <filesystem>
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

vc::PipelineConfig MakeBaseConfig(const std::filesystem::path& output_root) {
  vc::PipelineConfig config;
  config.dataset_root = "/workspace/GACRT024_1754812994";
  config.output_root = output_root.string();
  config.paths.dataset_root = config.dataset_root;
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
  vc::PipelineConfig config = MakeBaseConfig(root);
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
    vc::RunVirtualCameraPipeline(config);
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
  vc::PipelineConfig config = MakeBaseConfig(root);
  config.virtual_camera_parallelism = 2;
  config.virtual_tasks.push_back(
      MakeDuplicateJsonTask("cam.json", "json_collision_a/", "json_collision_a"));
  config.virtual_tasks.push_back(
      MakeDuplicateJsonTask("./cam.json", "json_collision_b/", "json_collision_b"));

  bool thrown = false;
  try {
    vc::RunVirtualCameraPipeline(config);
  } catch (const std::runtime_error& error) {
    const std::string message = error.what();
    thrown = message.find("duplicate virtual camera output json path") !=
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
  vc::PipelineConfig config = MakeBaseConfig(root);
  config.virtual_camera_parallelism = 2;
  config.virtual_tasks.push_back(
      MakeDuplicateImageTask("front_wide/", "image_collision_a.json",
                             "image_collision_a"));
  config.virtual_tasks.push_back(
      MakeDuplicateImageTask("front_wide/", "image_collision_b.json",
                             "image_collision_b"));

  bool thrown = false;
  try {
    vc::RunVirtualCameraPipeline(config);
  } catch (const std::runtime_error& error) {
    const std::string message = error.what();
    thrown = message.find("duplicate virtual camera output image path") !=
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
  vc::PipelineConfig config = MakeBaseConfig(root);
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

  vc::RunVirtualCameraPipeline(config);

  Expect(std::filesystem::exists(root / "image_virtual_camera" / "shared_output" /
                                 "left_1754812994899000000_50_0.jpg"),
         "left prefix output should exist");
  Expect(std::filesystem::exists(root / "image_virtual_camera" / "shared_output" /
                                 "right_1754812994899000000_50_0.jpg"),
         "right prefix output should exist");
}

void TestRunVirtualCameraPipelineRejectsDuplicateMapOutputs() {
  const std::filesystem::path root = MakeTestRoot("parallel_map_collision");
  vc::PipelineConfig config = MakeBaseConfig(root);
  config.virtual_camera_parallelism = 2;
  config.virtual_tasks.push_back(MakeDuplicateMapTask(
      "fw110_vc_mapX.bin", "map_collision_a/", "map_collision_a.json",
      "map_collision_a"));
  config.virtual_tasks.push_back(MakeDuplicateMapTask(
      "./fw110_vc_mapX.bin", "map_collision_b/", "map_collision_b.json",
      "map_collision_b"));

  bool thrown = false;
  try {
    vc::RunVirtualCameraPipeline(config);
  } catch (const std::runtime_error& error) {
    const std::string message = error.what();
    thrown = message.find("duplicate virtual camera output map path") !=
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

}  // namespace

int main() {
  TestRunVirtualCameraPipelineSerialStopsAfterFirstFailure();
  TestRunVirtualCameraPipelineRejectsDuplicateJsonOutputs();
  TestRunVirtualCameraPipelineRejectsDuplicateImageOutputs();
  TestRunVirtualCameraPipelineAllowsSameSaveDirWithDifferentPrefixes();
  TestRunVirtualCameraPipelineRejectsDuplicateMapOutputs();
  return 0;
}
