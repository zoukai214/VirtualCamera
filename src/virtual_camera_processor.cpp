#include "virtual_camera/virtual_camera_processor.h"

#include "virtual_camera/calibration_loader.h"
#include "virtual_camera/jobs.h"
#include "virtual_camera/json_utils.h"
#include "virtual_camera/json_writer.h"
#include "virtual_camera/map_generator.h"

#include <gen_vc_map.hpp>

#include <algorithm>
#include <filesystem>
#include <functional>
#include <fstream>
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

void FillCameraMatrix(const Eigen::Matrix3d& intrinsic, double matrix[3][3]) {
  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 3; ++col) {
      matrix[row][col] = intrinsic(row, col);
    }
  }
}

void FillExtrinsic(const Eigen::Matrix4d& extrinsic, double matrix[4][4]) {
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col) {
      matrix[row][col] = extrinsic(row, col);
    }
  }
}

void FillDistortion(const std::vector<double>& distortion, double values[8]) {
  std::fill(values, values + 8, 0.0);
  for (std::size_t index = 0; index < distortion.size() && index < 8; ++index) {
    values[index] = distortion[index];
  }
}

void SaveFloatMap(const std::filesystem::path& path, const cv::Mat& map) {
  EnsureDirectory(path.parent_path().string());
  cv::Mat contiguous = map;
  if (!contiguous.isContinuous()) {
    contiguous = map.clone();
  }
  std::ofstream output(path, std::ios::binary);
  if (!output.is_open()) {
    throw std::runtime_error("failed to write map: " + path.string());
  }
  output.write(reinterpret_cast<const char*>(contiguous.ptr<float>(0)),
               static_cast<std::streamsize>(contiguous.total() * sizeof(float)));
}

std::string NormalizedDestination(const std::filesystem::path& path) {
  return path.lexically_normal().string();
}

CalibrationParam LoadVirtualSourceCalibration(const PipelineConfig& config,
                                             const VirtualCameraTaskConfig& task) {
  const std::filesystem::path calib_dir =
      std::filesystem::path(config.dataset_root) / config.paths.conf_dir_path;
  const bool use_undistort_variant =
      config.undistort_image == 1 ||
      (config.undistort_image == 2 && task.undistort_image != 0);
  if (use_undistort_variant) {
    try {
      return LoadRt024Calibration(calib_dir.string(), task.conf_json,
                                  task.conf_intri_key + "-undistort",
                                  task.conf_extri_key + "-undistort",
                                  config.distort_model);
    } catch (const std::exception&) {
    }
  }
  return LoadRt024Calibration(calib_dir.string(), task.conf_json, task.conf_intri_key,
                              task.conf_extri_key, config.distort_model);
}

VirtualParam BuildVirtualParam(const VirtualCameraTaskConfig& task,
                               const CalibrationParam& calibration) {
  VirtualParam param;
  param.virtual_width = task.new_intrinsic.image_width;
  param.virtual_height = task.new_intrinsic.image_height;
  param.virtual_fov = task.new_intrinsic.fov;
  param.virtual_yaw = task.new_extrinsics.yaw;
  param.virtual_pitch = task.new_extrinsics.pitch;
  param.virtual_roll = task.new_extrinsics.roll;
  param.tx = calibration.extrinsic_matrix(0, 3);
  param.ty = calibration.extrinsic_matrix(1, 3);
  param.tz = calibration.extrinsic_matrix(2, 3);
  param.image_width = task.image_width;
  param.image_height = task.image_height;
  param.fov = task.fov;
  param.camera_id = task.camera_id;
  return param;
}

void RunVirtualCameraTask(const PipelineConfig& config,
                          const VirtualCameraTaskConfig& task) {
  CalibrationParam calibration = LoadVirtualSourceCalibration(config, task);

  double k_array[3][3];
  double d_array[8];
  double rt_array[4][4];
  FillCameraMatrix(calibration.intrinsic_matrix, k_array);
  FillDistortion(calibration.dist_data, d_array);
  FillExtrinsic(calibration.extrinsic_matrix, rt_array);

  cv::Mat map_x;
  cv::Mat map_y;
  cv::Mat src_map_x;
  cv::Mat src_map_y;
  if (config.distort_model == 1) {
    gen_vc_map_kb(k_array, d_array, rt_array, task.new_intrinsic.image_width,
                  task.new_intrinsic.image_height, task.new_intrinsic.fov,
                  task.new_extrinsics.yaw, map_x, map_y, config.showinfo, 1,
                  task.image_width, task.image_height, src_map_x, src_map_y);
  } else {
    gen_vc_map(k_array, d_array, rt_array, task.new_intrinsic.image_width,
               task.new_intrinsic.image_height, task.new_intrinsic.fov,
               task.new_extrinsics.yaw, map_x, map_y, config.showinfo, 1,
               task.image_width, task.image_height, src_map_x, src_map_y);
  }

  const std::filesystem::path map_root =
      std::filesystem::path(config.output_root) / config.paths.vc_gdcbin_dir_path;
  SaveFloatMap(map_root / task.vc_mapx_name, map_x);
  SaveFloatMap(map_root / task.vc_mapy_name, map_y);
  SaveFloatMap(map_root / task.src2vc_mapx_name, src_map_x);
  SaveFloatMap(map_root / task.src2vc_mapy_name, src_map_y);

  VirtualParam virtual_param = BuildVirtualParam(task, calibration);
  MapGenerator generator(calibration, virtual_param);
  const std::filesystem::path json_output =
      std::filesystem::path(config.output_root) / config.paths.vc_conf_dir_path /
      task.calib_json;
  WriteRt024VirtualJson(json_output.string(), generator.virtual_intrinsic(),
                        generator.virtual_extrinsic(), std::vector<double>(8, 0.0));

  const std::filesystem::path input_dir =
      std::filesystem::path(config.dataset_root) / config.paths.image_dir_path / task.image_dir;
  const std::filesystem::path output_dir =
      std::filesystem::path(config.output_root) / config.paths.vc_image_dir_path /
      task.save_dir;
  EnsureDirectory(output_dir.string());
  for (const auto& path : ListFiles(input_dir)) {
    const cv::Mat image = cv::imread(path.string(), cv::IMREAD_COLOR);
    if (image.empty()) {
      throw std::runtime_error("failed to read image: " + path.string());
    }
    cv::Mat remapped;
    cv::remap(image, remapped, map_x, map_y, cv::INTER_LINEAR);
    const std::string output_name =
        task.file_prefix + "_" + path.filename().string();
    if (!cv::imwrite((output_dir / output_name).string(), remapped)) {
      throw std::runtime_error("failed to write image: " +
                               (output_dir / output_name).string());
    }
  }
}

void ValidateVirtualCameraOutputs(const PipelineConfig& config) {
  std::unordered_set<std::string> outputs;

  auto RegisterOutput = [&outputs](const std::filesystem::path& path) {
    const std::string normalized_output = NormalizedDestination(path);
    if (!outputs.insert(normalized_output).second) {
      throw std::runtime_error("duplicate virtual camera output path: " +
                               normalized_output);
    }
  };

  for (const auto& task : config.virtual_tasks) {
    const std::filesystem::path map_root =
        std::filesystem::path(config.output_root) / config.paths.vc_gdcbin_dir_path;
    RegisterOutput(map_root / task.vc_mapx_name);
    RegisterOutput(map_root / task.vc_mapy_name);
    RegisterOutput(map_root / task.src2vc_mapx_name);
    RegisterOutput(map_root / task.src2vc_mapy_name);

    const std::filesystem::path json_output =
        std::filesystem::path(config.output_root) / config.paths.vc_conf_dir_path /
        task.calib_json;
    RegisterOutput(json_output);

    const std::filesystem::path input_dir =
        std::filesystem::path(config.dataset_root) / config.paths.image_dir_path /
        task.image_dir;
    const std::filesystem::path image_root =
        std::filesystem::path(config.output_root) / config.paths.vc_image_dir_path /
        task.save_dir;
    for (const auto& input_path : ListFiles(input_dir)) {
      RegisterOutput(image_root / (task.file_prefix + "_" + input_path.filename().string()));
    }
  }
}

}  // namespace

void RunVirtualCameraPipeline(const PipelineConfig& config) {
  if (config.virtual_camera_parallelism <= 1) {
    for (const auto& task : config.virtual_tasks) {
      RunVirtualCameraTask(config, task);
    }
    return;
  }

  ValidateVirtualCameraOutputs(config);

  std::vector<std::function<void()>> jobs;
  jobs.reserve(config.virtual_tasks.size());
  for (const auto& task : config.virtual_tasks) {
    jobs.push_back([&config, task]() { RunVirtualCameraTask(config, task); });
  }
  RunJobs(jobs, config.virtual_camera_parallelism);
}

}  // namespace vc
