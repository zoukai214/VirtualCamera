#include "virtual_camera/four_view_runner.h"

#include "bin_file_io.h"
#include "camera_maps_generator.h"
#include "camera_params_loader.h"
#include "config_loader.h"
#include "cylinder_map.h"

#include <opencv2/core.hpp>

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace vc {
namespace {

void GenerateCylinderMaps(const ConfigLoader::CylinderConfig& cylinder_config,
                          const std::vector<CameraModelExt>& cam_model_ext,
                          const std::vector<CameraModelInt>& cam_model_int_src) {
  if (!cylinder_config.enabled) {
    return;
  }
  if (cylinder_config.output_dir.empty()) {
    throw std::runtime_error("cylinder.output_dir is empty");
  }

  std::filesystem::create_directories(cylinder_config.output_dir);

  struct CylinderCamInfo {
    int loader_idx;
    int cylinder_cam_id;
    std::string name;
  };
  const CylinderCamInfo cam_infos[] = {
      {1, 0, "front"},
      {0, 1, "left"},
      {2, 2, "right"},
      {3, 3, "rear"},
  };

  for (const auto& cam : cam_infos) {
    double k_array[3][3] = {};
    const auto& intrin = cam_model_int_src.at(cam.loader_idx).intrin;
    for (int row = 0; row < 3; ++row) {
      for (int col = 0; col < 3; ++col) {
        k_array[row][col] = intrin[row * 3 + col];
      }
    }

    double d_array[4] = {};
    const auto& distortion = cam_model_int_src.at(cam.loader_idx).distortion_coeff;
    for (int index = 0; index < 4; ++index) {
      d_array[index] = distortion.at(index);
    }

    double rt_array[4][4] = {};
    const cv::Mat& rotation = cam_model_ext.at(cam.loader_idx).rotation;
    const cv::Mat& translation = cam_model_ext.at(cam.loader_idx).translation;
    for (int row = 0; row < 3; ++row) {
      for (int col = 0; col < 3; ++col) {
        rt_array[row][col] = rotation.at<double>(row, col);
      }
      rt_array[row][3] = translation.at<double>(row, 0);
    }
    rt_array[3][3] = 1.0;

    cv::Mat cyl_map_x;
    cv::Mat cyl_map_y;
    fisheye_cylinder::genCylinderMap(
        k_array, d_array, rt_array, cam.cylinder_cam_id, cylinder_config.width,
        cylinder_config.height, cylinder_config.fx, cylinder_config.fy,
        cylinder_config.cx, cylinder_config.cy, cylinder_config.radius, 1920,
        1536, cyl_map_x, cyl_map_y);

    const std::string bin_path =
        (std::filesystem::path(cylinder_config.output_dir) /
         ("gdc_" + cam.name + ".bin"))
            .string();
    if (!fisheye_cylinder::saveCylinderMapBin(cyl_map_x, cyl_map_y, bin_path)) {
      throw std::runtime_error("failed to save cylinder map: " + bin_path);
    }
  }
}

}  // namespace

int RunFourViewGenerate(const std::string& config_path) {
  ConfigLoader config_loader(config_path);
  if (!config_loader.validateConfig()) {
    throw std::runtime_error("4v config validation failed: " + config_path);
  }

  const auto input_config = config_loader.getInputConfig();
  const auto output_config = config_loader.getOutputConfig();
  const auto stitching_config = config_loader.getStitchingConfig();
  const auto result_param = config_loader.getResultSizeParam();
  const auto image_config = config_loader.getImageConfig();

  std::vector<CameraModelExt> cam_model_ext;
  std::vector<CameraModelInt> cam_model_int;
  std::vector<CameraModelInt> cam_model_int_src;
  if (!CameraParamsLoader::loadCameraParams(
          input_config.camera_params_file, input_config.image_params.count,
          cam_model_ext, cam_model_int, cam_model_int_src)) {
    throw std::runtime_error("failed to load 4v camera params: " +
                             input_config.camera_params_file);
  }

  const double vehicle_width = image_config.vehicle.width / 1000.0;
  const double vehicle_length = image_config.vehicle.length / 1000.0;
  const double vehicle_overhang = image_config.vehicle.overhang / 1000.0;

  const CameraMaps camera_maps = CameraMapsGenerator::generateCameraMapsOptimized(
      result_param, stitching_config, cam_model_ext, cam_model_int,
      input_config.image_params.image_width, input_config.image_params.image_height,
      vehicle_width, vehicle_length, vehicle_overhang);

  if (output_config.output_format != "bin" && output_config.output_format != "both") {
    throw std::runtime_error("generate-4v supports bin output only in this project");
  }
  if (!BinFileIO::saveCameraMaps(camera_maps, output_config.camera_maps_file)) {
    throw std::runtime_error("failed to save 4v maps: " +
                             output_config.camera_maps_file);
  }

  GenerateCylinderMaps(config_loader.getCylinderConfig(), cam_model_ext, cam_model_int_src);
  std::cout << "4v generation passed\n";
  return 0;
}

}  // namespace vc
