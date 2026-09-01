#include "virtual_camera/virtual_camera_processor.h"

#include "virtual_camera/calibration_loader.h"
#include "virtual_camera/cuda_remap.h"
#include "virtual_camera/jobs.h"
#include "virtual_camera/json_utils.h"
#include "virtual_camera/json_writer.h"
#include "virtual_camera/logging.h"
#include "virtual_camera/map_generator.h"
#include "virtual_camera/remap_generator.h"

#include <opencv2/imgcodecs.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <functional>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
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

std::string NormalizedDestination(const std::filesystem::path& path) {
  return path.lexically_normal().string();
}

CalibrationParam LoadVirtualSourceCalibration(const PipelineConfig& config,
                                             const std::string& dataset_root,
                                             const VirtualCameraTaskConfig& task) {
  const std::filesystem::path calib_dir =
      std::filesystem::path(dataset_root) / config.paths.conf_dir_path;
  const bool use_undistort_variant =
      config.undistort_image == 1 ||
      (config.undistort_image == 2 && task.undistort_image != 0);
  if (use_undistort_variant) {
    try {
      return LoadCalibration(calib_dir.string(), task.conf_json,
                                  task.conf_intri_key + "-undistort",
                                  task.conf_extri_key + "-undistort",
                                  config.distort_model);
    } catch (const std::exception&) {
    }
  }
  return LoadCalibration(calib_dir.string(), task.conf_json, task.conf_intri_key,
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

VirtualCameraCacheEntry BuildVirtualCameraCacheEntry(
    const PipelineConfig& config, const std::string& dataset_root,
    const VirtualCameraTaskConfig& task) {
  CalibrationParam calibration =
      LoadVirtualSourceCalibration(config, dataset_root, task);
  const VirtualCameraMaps maps =
      GenerateVirtualCameraMaps(calibration, task, config.distort_model);
  VirtualParam virtual_param = BuildVirtualParam(task, calibration);
  MapGenerator generator(calibration, virtual_param);

  VirtualCameraCacheEntry entry;
  entry.task = task;
  entry.maps = maps;
  entry.gpu_maps = UploadCudaRemapMaps(entry.maps.map_x, entry.maps.map_y);
  entry.virtual_intrinsic = generator.virtual_intrinsic();
  entry.virtual_extrinsic = generator.virtual_extrinsic();
  entry.dist_data = std::vector<double>(8, 0.0);
  return entry;
}

void SaveVirtualCameraMapArtifacts(const PipelineConfig& config,
                                   const VirtualCameraCacheEntry& entry,
                                   const std::string& output_root) {
  const VirtualCameraTaskConfig& task = entry.task;
  const std::filesystem::path map_root =
      std::filesystem::path(output_root) / config.paths.vc_gdcbin_dir_path;
  SaveFloatMapFile((map_root / task.vc_mapx_name).string(), entry.maps.map_x);
  SaveFloatMapFile((map_root / task.vc_mapy_name).string(), entry.maps.map_y);
  SaveFloatMapFile((map_root / task.src2vc_mapx_name).string(),
                   entry.maps.src_map_x);
  SaveFloatMapFile((map_root / task.src2vc_mapy_name).string(),
                   entry.maps.src_map_y);
}

void SaveVirtualCameraJsonArtifact(const PipelineConfig& config,
                                   const VirtualCameraCacheEntry& entry,
                                   const std::string& output_root) {
  const VirtualCameraTaskConfig& task = entry.task;
  const std::filesystem::path json_output =
      std::filesystem::path(output_root) / config.paths.vc_conf_dir_path /
      task.calib_json;
  WriteVirtualJson(json_output.string(), entry.virtual_intrinsic,
                   entry.virtual_extrinsic, entry.dist_data);
}

struct SourceCameraInput {
  int camera_id = 0;
  std::string image_dir;
};

std::vector<SourceCameraInput> BuildSourceCameraInputs(
    const std::vector<VirtualCameraTaskConfig>& tasks) {
  std::vector<SourceCameraInput> inputs;
  std::unordered_set<int> seen_camera_ids;
  for (const auto& task : tasks) {
    if (!seen_camera_ids.insert(task.camera_id).second) {
      continue;
    }
    inputs.push_back(SourceCameraInput{task.camera_id, task.image_dir});
  }
  return inputs;
}

void ValidateVirtualCameraOutputDestinations(const PipelineConfig& config,
                                             const std::string& output_root) {
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
        std::filesystem::path(output_root) / config.paths.vc_gdcbin_dir_path;
    RegisterOutput(map_root / task.vc_mapx_name);
    RegisterOutput(map_root / task.vc_mapy_name);
    RegisterOutput(map_root / task.src2vc_mapx_name);
    RegisterOutput(map_root / task.src2vc_mapy_name);

    const std::filesystem::path json_output =
        std::filesystem::path(output_root) / config.paths.vc_conf_dir_path /
        task.calib_json;
    RegisterOutput(json_output);

    const std::filesystem::path image_root =
        std::filesystem::path(output_root) / config.paths.vc_image_dir_path /
        task.save_dir;
    RegisterOutput(image_root / (task.file_prefix + "_*"));
  }
}

}  // namespace

VirtualCameraCache BuildVirtualCameraCache(const PipelineConfig& config,
                                           const std::string& dataset_root) {
  VirtualCameraCache cache;
  cache.entries.reserve(config.virtual_tasks.size());
  for (const auto& task : config.virtual_tasks) {
    VirtualCameraCacheEntry entry =
        BuildVirtualCameraCacheEntry(config, dataset_root, task);
    const std::size_t entry_index = cache.entries.size();
    cache.entries.push_back(std::move(entry));
    cache.entries_by_camera_id[task.camera_id].push_back(entry_index);
  }
  return cache;
}

void SaveVirtualCameraCacheArtifacts(const PipelineConfig& config,
                                     const VirtualCameraCache& cache,
                                     const std::string& output_root) {
  for (const auto& entry : cache.entries) {
    SaveVirtualCameraMapArtifacts(config, entry, output_root);
    SaveVirtualCameraJsonArtifact(config, entry, output_root);
  }
}

std::vector<VirtualCameraFrameResult> ProcessVirtualCameraFrame(
    const VirtualCameraCache& cache, int camera_id, const GpuImage& image) {
  if (image.image.empty()) {
    throw std::runtime_error("GPU image must not be empty");
  }

  std::vector<VirtualCameraFrameResult> results;
  const auto found = cache.entries_by_camera_id.find(camera_id);
  if (found == cache.entries_by_camera_id.end()) {
    return results;
  }

  results.reserve(found->second.size());
  for (const std::size_t entry_index : found->second) {
    const VirtualCameraCacheEntry& entry = cache.entries.at(entry_index);
    VirtualCameraFrameResult result;
    result.task = entry.task;
    result.image.image = GpuRemap(image.image, entry.gpu_maps);
    results.push_back(std::move(result));
  }
  return results;
}

void SaveVirtualCameraFrameResult(const PipelineConfig& config,
                                  const VirtualCameraFrameResult& result,
                                  const std::string& output_root,
                                  const std::string& input_filename) {
  const std::filesystem::path output_dir =
      std::filesystem::path(output_root) / config.paths.vc_image_dir_path /
      result.task.save_dir;
  EnsureDirectory(output_dir.string());
  const std::string output_name = result.task.file_prefix + "_" + input_filename;
  if (result.image.image.empty()) {
    throw std::runtime_error("GPU result image must not be empty");
  }
  const std::string output_path = (output_dir / output_name).string();
  cv::Mat cpu_image;
  result.image.image.download(cpu_image);
  if (!cv::imwrite(output_path, cpu_image)) {
    throw std::runtime_error("failed to write image: " +
                             output_path);
  }
}

void ValidateVirtualCameraOutputs(const PipelineConfig& config,
                                  const std::string& output_root) {
  ValidateVirtualCameraOutputDestinations(config, output_root);
}

void RunVirtualCameraPipeline(const PipelineConfig& config,
                              const std::string& dataset_root,
                              const std::string& output_root) {
  const auto start_time = std::chrono::steady_clock::now();
  LogInfo(config.showinfo != 0, BuildVirtualCameraPipelineStartMessage(config));
  RequireCudaRemapAvailable();

  ValidateVirtualCameraOutputs(config, output_root);

  std::vector<std::chrono::steady_clock::time_point> task_start_times;
  task_start_times.reserve(config.virtual_tasks.size());
  for (const auto& task : config.virtual_tasks) {
    task_start_times.push_back(std::chrono::steady_clock::now());
    LogInfo(config.showinfo != 0, BuildVirtualTaskStartMessage(task));
  }

  const VirtualCameraCache cache = BuildVirtualCameraCache(config, dataset_root);
  SaveVirtualCameraCacheArtifacts(config, cache, output_root);

  const auto source_inputs = BuildSourceCameraInputs(config.virtual_tasks);
  auto process_source_camera =
      [&config, &cache, &dataset_root, &output_root](
          const SourceCameraInput& input) {
    const std::filesystem::path input_dir =
        std::filesystem::path(dataset_root) / config.paths.image_dir_path /
        input.image_dir;
    for (const auto& path : ListFiles(input_dir)) {
      const GpuImage image = ReadImage(path.string());
      const std::vector<VirtualCameraFrameResult> results =
          ProcessVirtualCameraFrame(cache, input.camera_id, image);
      for (const auto& result : results) {
        SaveVirtualCameraFrameResult(config, result, output_root,
                                     path.filename().string());
      }
    }
  };

  if (config.virtual_camera_parallelism <= 1) {
    for (const auto& input : source_inputs) {
      process_source_camera(input);
    }
  } else {
    std::vector<std::function<void()>> jobs;
    jobs.reserve(source_inputs.size());
    for (const auto& input : source_inputs) {
      jobs.push_back([process_source_camera, input]() {
        process_source_camera(input);
      });
    }
    RunJobs(jobs, config.virtual_camera_parallelism);
  }

  for (std::size_t index = 0; index < config.virtual_tasks.size(); ++index) {
    LogInfo(config.showinfo != 0,
            BuildVirtualTaskDoneMessage(
                config.virtual_tasks.at(index),
                ElapsedMilliseconds(task_start_times.at(index))));
  }
  LogInfo(config.showinfo != 0,
          BuildVirtualCameraPipelineDoneMessage(
              ElapsedMilliseconds(start_time)));
}

}  // namespace vc
