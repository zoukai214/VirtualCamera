#include "virtual_camera/undistort_processor.h"

#include "virtual_camera/calibration_loader.h"
#include "virtual_camera/json_utils.h"
#include "virtual_camera/json_writer.h"

#include <gen_vc_map.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace vc {
namespace {

std::vector<std::filesystem::path> ListFiles(const std::filesystem::path& dir) {
  std::vector<std::filesystem::path> files;
  for (const auto& entry : std::filesystem::directory_iterator(dir)) {
    if (entry.is_regular_file()) {
      files.push_back(entry.path());
    }
  }
  std::sort(files.begin(), files.end());
  return files;
}

void FillCameraMatrix(const Eigen::Matrix3d& intrinsic, double matrix[3][3]) {
  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 3; ++col) {
      matrix[row][col] = intrinsic(row, col);
    }
  }
}

void FillDistortion(const std::vector<double>& distortion, double values[8]) {
  std::fill(values, values + 8, 0.0);
  for (std::size_t index = 0; index < distortion.size() && index < 8; ++index) {
    values[index] = distortion[index];
  }
}

CalibrationParam LoadUndistortSourceCalibration(const PipelineConfig& config,
                                                const UndistortTaskConfig& task) {
  return LoadRt024Calibration(
      (std::filesystem::path(config.dataset_root) / config.paths.conf_dir_path).string(),
      task.conf_json, task.intri_key, task.extri_key, config.distort_model);
}

}  // namespace

void RunUndistortPipeline(const PipelineConfig& config) {
  for (const auto& task : config.undistort_tasks) {
    CalibrationParam calibration = LoadUndistortSourceCalibration(config, task);

    double k_array[3][3];
    double d_array[8];
    double new_k_array[4];
    FillCameraMatrix(calibration.intrinsic_matrix, k_array);
    FillDistortion(calibration.dist_data, d_array);
    new_k_array[0] = task.new_intrinsic.focal_u;
    new_k_array[1] = task.new_intrinsic.center == 0 ? calibration.intrinsic_matrix(0, 2)
                                                     : task.new_intrinsic.center_u;
    new_k_array[2] = task.new_intrinsic.focal_v;
    new_k_array[3] = task.new_intrinsic.center == 0 ? calibration.intrinsic_matrix(1, 2)
                                                     : task.new_intrinsic.center_v;

    cv::Mat map_x;
    cv::Mat map_y;
    if (config.distort_model == 1) {
      gen_undis_map_kb(k_array, d_array, calibration.image_width, calibration.image_height,
                       new_k_array, map_x, map_y, config.showinfo);
    } else {
      gen_undis_map(k_array, d_array, calibration.image_width, calibration.image_height,
                    new_k_array, map_x, map_y, config.showinfo);
    }

    const std::filesystem::path json_output =
        std::filesystem::path(config.output_root) /
        config.paths.undistort_conf_dir_path / task.conf_json;
    WriteRt024UndistortJson(json_output.string(), calibration, task.new_intrinsic);

    const std::filesystem::path input_dir =
        std::filesystem::path(config.dataset_root) / config.paths.image_dir_path / task.image_dir;
    const std::filesystem::path output_dir =
        std::filesystem::path(config.output_root) /
        config.paths.undistort_image_dir_path / task.image_dir;
    EnsureDirectory(output_dir.string());
    for (const auto& path : ListFiles(input_dir)) {
      const cv::Mat image = cv::imread(path.string(), cv::IMREAD_COLOR);
      if (image.empty()) {
        throw std::runtime_error("failed to read image: " + path.string());
      }
      cv::Mat remapped;
      cv::remap(image, remapped, map_x, map_y, cv::INTER_LINEAR);
      if (!cv::imwrite((output_dir / path.filename()).string(), remapped)) {
        throw std::runtime_error("failed to write image: " +
                                 (output_dir / path.filename()).string());
      }
    }
  }
}

}  // namespace vc
