#include "virtual_camera/undistort_processor.h"

#include "virtual_camera/calibration_loader.h"
#include "virtual_camera/jobs.h"
#include "virtual_camera/json_utils.h"
#include "virtual_camera/json_writer.h"
#include "virtual_camera/remap_generator.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_set>
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

CalibrationParam LoadUndistortSourceCalibration(const PipelineConfig& config,
                                                const UndistortTaskConfig& task) {
  return LoadRt024Calibration(
      (std::filesystem::path(config.dataset_root) / config.paths.conf_dir_path).string(),
      task.conf_json, task.intri_key, task.extri_key, config.distort_model);
}

void RunUndistortTask(const PipelineConfig& config, const UndistortTaskConfig& task) {
  CalibrationParam calibration = LoadUndistortSourceCalibration(config, task);
  const UndistortMaps maps =
      GenerateUndistortMaps(calibration, task.new_intrinsic, config.distort_model);

  const std::filesystem::path json_output =
      std::filesystem::path(config.output_root) / config.paths.undistort_conf_dir_path /
      task.conf_json;
  WriteRt024UndistortJson(json_output.string(), calibration, task.new_intrinsic);

  const std::filesystem::path input_dir =
      std::filesystem::path(config.dataset_root) / config.paths.image_dir_path / task.image_dir;
  const std::filesystem::path output_dir =
      std::filesystem::path(config.output_root) / config.paths.undistort_image_dir_path /
      task.image_dir;
  EnsureDirectory(output_dir.string());
  for (const auto& path : ListFiles(input_dir)) {
    const cv::Mat image = cv::imread(path.string(), cv::IMREAD_COLOR);
    if (image.empty()) {
      throw std::runtime_error("failed to read image: " + path.string());
    }
    cv::Mat remapped;
    cv::remap(image, remapped, maps.map_x, maps.map_y, cv::INTER_LINEAR);
    if (!cv::imwrite((output_dir / path.filename()).string(), remapped)) {
      throw std::runtime_error("failed to write image: " +
                               (output_dir / path.filename()).string());
    }
  }
}

void ValidateUndistortOutputs(const PipelineConfig& config) {
  std::unordered_set<std::string> conf_outputs;
  std::unordered_set<std::string> image_outputs;
  for (const auto& task : config.undistort_tasks) {
    const std::string conf_output = (std::filesystem::path(config.output_root) /
                                     config.paths.undistort_conf_dir_path /
                                     task.conf_json)
                                        .lexically_normal()
                                        .string();
    if (!conf_outputs.insert(conf_output).second) {
      throw std::runtime_error("duplicate undistort output json path: " + conf_output);
    }
    const std::string image_output = (std::filesystem::path(config.output_root) /
                                      config.paths.undistort_image_dir_path /
                                      task.image_dir)
                                         .lexically_normal()
                                         .string();
    if (!image_outputs.insert(image_output).second) {
      throw std::runtime_error("duplicate undistort output image path: " + image_output);
    }
  }
}

}  // namespace

void RunUndistortPipeline(const PipelineConfig& config) {
  if (config.undistort_parallelism <= 1) {
    for (const auto& task : config.undistort_tasks) {
      RunUndistortTask(config, task);
    }
    return;
  }

  ValidateUndistortOutputs(config);

  std::vector<std::function<void()>> jobs;
  jobs.reserve(config.undistort_tasks.size());
  for (const auto& task : config.undistort_tasks) {
    jobs.push_back([&config, task]() { RunUndistortTask(config, task); });
  }
  RunJobs(jobs, config.undistort_parallelism);
}

}  // namespace vc
