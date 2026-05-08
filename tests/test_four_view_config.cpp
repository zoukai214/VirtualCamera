#include "config_loader.h"
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

bool TestRelativeOutputPathsResolveFromConfigDir() {
  const std::filesystem::path root = "build/test_tmp/four_view_config";
  std::filesystem::remove_all(root);
  WriteText(root / "cfg/config.yaml",
            "input:\n"
            "  camera_params_file: fisheye_cam_param.json\n"
            "  image_params:\n"
            "    count: 4\n"
            "    image_width: 1280\n"
            "    image_height: 800\n"
            "output:\n"
            "  camera_maps_file: maps\n"
            "  stitched_result: stitched.jpg\n"
            "  output_format: bin\n"
            "stitching:\n"
            "  visual_world:\n"
            "    width: 12.0\n"
            "    height: 12.0\n"
            "  fusion:\n"
            "    parallel_range: 20.0\n"
            "    curve_range: 20.0\n"
            "    angles:\n"
            "      front_left: 75.0\n"
            "      front_right: 75.0\n"
            "      rear_left: 75.0\n"
            "      rear_right: 75.0\n"
            "image:\n"
            "  output_size:\n"
            "    width: 640\n"
            "    height: 640\n"
            "  vehicle:\n"
            "    width: 1950\n"
            "    length: 4855\n"
            "    overhang: 1068\n"
            "cylinder:\n"
            "  enabled: true\n"
            "  width: 768\n"
            "  height: 512\n"
            "  fx: 229.2\n"
            "  fy: 229.2\n"
            "  cx: 384.0\n"
            "  cy: 224.0\n"
            "  radius: 10000.0\n"
            "  output_dir: gdc\n");

  ConfigLoader loader((root / "cfg/config.yaml").string());
  const std::filesystem::path expected_dir = std::filesystem::absolute(root / "cfg");
  const auto input = loader.getInputConfig();
  const auto output = loader.getOutputConfig();
  const auto cylinder = loader.getCylinderConfig();

  if (input.camera_params_file != (expected_dir / "fisheye_cam_param.json").string()) {
    std::cerr << "input path did not resolve from config directory\n";
    return false;
  }
  if (output.camera_maps_file != (expected_dir / "maps").string()) {
    std::cerr << "output map path did not resolve from config directory\n";
    return false;
  }
  if (output.stitched_result != (expected_dir / "stitched.jpg").string()) {
    std::cerr << "stitched result path did not resolve from config directory\n";
    return false;
  }
  if (!cylinder.enabled || cylinder.output_dir != (expected_dir / "gdc").string()) {
    std::cerr << "cylinder output path did not resolve from config directory\n";
    return false;
  }
  return true;
}

bool TestMissingConfigThrows() {
  try {
    ConfigLoader loader("build/test_tmp/missing_4v_config.yaml");
    (void)loader;
  } catch (const std::runtime_error& ex) {
    return std::string(ex.what()).find("配置文件不存在") != std::string::npos;
  }
  std::cerr << "expected missing config exception\n";
  return false;
}

}  // namespace

int main() {
  if (!TestRelativeOutputPathsResolveFromConfigDir()) {
    return 1;
  }
  if (!TestMissingConfigThrows()) {
    return 1;
  }
  return 0;
}
