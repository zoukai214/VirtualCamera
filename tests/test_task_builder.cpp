#include "virtual_camera/json_utils.h"
#include "virtual_camera/task_builder.h"

#include <filesystem>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>

namespace {

bool CheckThorTaskCounts(const std::vector<vc::CameraTask>& tasks,
                         const std::string& context) {
  int virtual_count = 0;
  int undistort_count = 0;
  int resize_count = 0;
  std::set<std::string> bin_names;

  for (const auto& task : tasks) {
    bin_names.insert(task.bin_name);
    if (task.type == vc::TaskType::kVirtual) {
      ++virtual_count;
    } else if (task.type == vc::TaskType::kUndistort) {
      ++undistort_count;
    } else if (task.type == vc::TaskType::kResize) {
      ++resize_count;
    }
  }

  if (virtual_count != 9) {
    std::cerr << context << ": expected 9 virtual tasks, got "
              << virtual_count << "\n";
    return false;
  }
  if (undistort_count != 7) {
    std::cerr << context << ": expected 7 undistort tasks, got "
              << undistort_count << "\n";
    return false;
  }
  if (resize_count != 3) {
    std::cerr << context << ": expected 3 resize tasks, got " << resize_count
              << "\n";
    return false;
  }
  if (!bin_names.count("gdc_ft30.bin") ||
      !bin_names.count("gdc_fw120_1080.bin")) {
    std::cerr << context << ": expected Thor bin names are missing\n";
    return false;
  }
  return true;
}

nlohmann::json MakeVirtualCamera(const std::string& conf_json, int camera_id,
                                 const std::string& bin_name,
                                 const std::string& undistort_bin_name) {
  return {
      {"fov", 30},
      {"image_height", 2160},
      {"image_width", 3840},
      {"desc", conf_json},
      {"conf_json", conf_json},
      {"intri_json", conf_json + "/" + conf_json + "-intrinsic.json"},
      {"extri_json",
       conf_json + "/" + conf_json + "-to-car_center-extrinsic.json"},
      {"camera_id", camera_id},
      {"bin_name", bin_name},
      {"undis_bin_name", undistort_bin_name},
      {"new_intrinsic",
       {{"fov", 30},
        {"focal_u", 1910.8},
        {"focal_v", 1910.8},
        {"center_u", 512.0},
        {"center_v", 256.0},
        {"image_width", 1024},
        {"image_height", 512}}},
      {"new_extrinsics", {{"pitch", 0.0}, {"roll", 0.0}, {"yaw", 0.0}}},
  };
}

bool TestFullConfigStillLoads() {
  const auto config = vc::LoadThorConfig("configs/config_thor.json");
  return CheckThorTaskCounts(vc::BuildThorTasks(config), "full config");
}

bool TestIncludeConfigMergesChildFiles() {
  const std::filesystem::path root = "build/test_tmp/include_config";
  std::filesystem::remove_all(root);
  vc::EnsureDirectory((root / "thor").string());

  vc::WriteJson(
      (root / "thor/common.json").string(),
      {{"showinfo", true}, {"virtual_camera_configs", nlohmann::json::array()}});
  vc::WriteJson(
      (root / "thor/virtual_a.json").string(),
      {{"virtual_camera_configs",
        nlohmann::json::array(
            {MakeVirtualCamera("front_a", 0, "gdc_front_a.bin",
                               "ldc_front_a.bin")})}});
  vc::WriteJson(
      (root / "thor/virtual_b.json").string(),
      {{"virtual_camera_configs",
        nlohmann::json::array(
            {MakeVirtualCamera("front_b", 1, "gdc_front_b.bin",
                               "ldc_front_b.bin")})}});
  vc::WriteJson(
      (root / "thor/resize.json").string(),
      {{"virtual_init_camera_config",
        nlohmann::json::array(
            {MakeVirtualCamera("resize_a", 2, "gdc_resize_a.bin", "")})}});
  vc::WriteJson(
      (root / "config.json").string(),
      {{"include",
        nlohmann::json::array({"thor/common.json", "thor/virtual_a.json",
                               "thor/virtual_b.json", "thor/resize.json"})}});

  const auto config = vc::LoadThorConfig((root / "config.json").string());
  const auto tasks = vc::BuildThorTasks(config);

  int virtual_count = 0;
  int undistort_count = 0;
  int resize_count = 0;
  std::set<std::string> bin_names;
  for (const auto& task : tasks) {
    bin_names.insert(task.bin_name);
    if (task.type == vc::TaskType::kVirtual) {
      ++virtual_count;
    } else if (task.type == vc::TaskType::kUndistort) {
      ++undistort_count;
    } else if (task.type == vc::TaskType::kResize) {
      ++resize_count;
    }
  }

  if (virtual_count != 2 || undistort_count != 2 || resize_count != 1) {
    std::cerr << "include config: unexpected task counts: virtual="
              << virtual_count << ", undistort=" << undistort_count
              << ", resize=" << resize_count << "\n";
    return false;
  }
  if (!bin_names.count("gdc_front_a.bin") ||
      !bin_names.count("ldc_front_b.bin") ||
      !bin_names.count("gdc_resize_a.bin")) {
    std::cerr << "include config: merged bin names are missing\n";
    return false;
  }
  return true;
}

bool TestInvalidIncludeTypeThrows() {
  const std::filesystem::path root = "build/test_tmp/invalid_include";
  std::filesystem::remove_all(root);
  vc::EnsureDirectory(root.string());
  vc::WriteJson((root / "config.json").string(),
                {{"include", "thor/common.json"}});

  try {
    (void)vc::LoadThorConfig((root / "config.json").string());
  } catch (const std::runtime_error& ex) {
    const std::string message = ex.what();
    if (message.find("include must be an array") != std::string::npos) {
      return true;
    }
    std::cerr << "invalid include: unexpected error: " << message << "\n";
    return false;
  }

  std::cerr << "invalid include: expected exception\n";
  return false;
}

}  // namespace

int main() {
  if (!TestFullConfigStillLoads()) {
    return 1;
  }
  if (!TestIncludeConfigMergesChildFiles()) {
    return 1;
  }
  if (!TestInvalidIncludeTypeThrows()) {
    return 1;
  }
  return 0;
}
