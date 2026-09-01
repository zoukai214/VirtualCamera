#include "virtual_camera/undistort_processor.h"

#include "virtual_camera/calibration_loader.h"
#include "virtual_camera/cuda_remap.h"
#include "virtual_camera/jobs.h"
#include "virtual_camera/json_utils.h"
#include "virtual_camera/json_writer.h"
#include "virtual_camera/logging.h"
#include "virtual_camera/remap_generator.h"
#include "virtual_camera/types.h"

#include <opencv2/imgcodecs.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <functional>
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

CalibrationParam LoadUndistortSourceCalibration(const PipelineConfig& config,
                                                const std::string& dataset_root,
                                                const UndistortTaskConfig& task) {
  return LoadCalibration(
      (std::filesystem::path(dataset_root) / config.paths.conf_dir_path).string(),
      task.conf_json, task.intri_key, task.extri_key, config.distort_model);
}

int CameraIdFromIntrinsicKey(const std::string& intri_key) {
  for (const auto& item : kCameraNames) {
    if (item.second == intri_key) {
      return item.first;
    }
  }
  throw std::runtime_error("unknown camera intrinsic key: " + intri_key);
}

UndistortCacheEntry BuildUndistortCacheEntry(const PipelineConfig& config,
                                             const std::string& dataset_root,
                                             const UndistortTaskConfig& task) {
  CalibrationParam calibration =
      LoadUndistortSourceCalibration(config, dataset_root, task);
  const UndistortMaps maps =
      GenerateUndistortMaps(calibration, task.new_intrinsic, config.distort_model);

  UndistortCacheEntry entry;
  entry.task = task;
  entry.camera_id = CameraIdFromIntrinsicKey(task.intri_key);
  entry.calibration = calibration;
  entry.maps = maps;
  entry.gpu_maps = UploadCudaRemapMaps(entry.maps.map_x, entry.maps.map_y);
  return entry;
}

void SaveUndistortJsonArtifact(const PipelineConfig& config,
                               const UndistortCacheEntry& entry,
                               const std::string& output_root) {
  const std::filesystem::path json_output =
      std::filesystem::path(output_root) / config.paths.undistort_conf_dir_path /
      entry.task.conf_json;
  WriteUndistortJson(json_output.string(), entry.calibration,
                     entry.task.new_intrinsic);
}

struct SourceCameraInput {
  int camera_id = 0;
  std::string image_dir;
};

std::vector<SourceCameraInput> BuildSourceCameraInputs(
    const std::vector<UndistortTaskConfig>& tasks) {
  std::vector<SourceCameraInput> inputs;
  std::unordered_set<int> seen_camera_ids;
  for (const auto& task : tasks) {
    const int camera_id = CameraIdFromIntrinsicKey(task.intri_key);
    if (!seen_camera_ids.insert(camera_id).second) {
      continue;
    }
    inputs.push_back(SourceCameraInput{camera_id, task.image_dir});
  }
  return inputs;
}

void ValidateUndistortOutputDestinations(const PipelineConfig& config,
                                         const std::string& output_root) {
  std::unordered_set<std::string> conf_outputs;
  std::unordered_set<std::string> image_outputs;
  for (const auto& task : config.undistort_tasks) {
    const std::string conf_output = (std::filesystem::path(output_root) /
                                     config.paths.undistort_conf_dir_path /
                                     task.conf_json)
                                        .lexically_normal()
                                        .string();
    if (!conf_outputs.insert(conf_output).second) {
      throw std::runtime_error("duplicate undistort output json path: " + conf_output);
    }
    const std::string image_output = (std::filesystem::path(output_root) /
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

UndistortCache BuildUndistortCache(const PipelineConfig& config,
                                   const std::string& dataset_root) {
  UndistortCache cache;
  cache.entries.reserve(config.undistort_tasks.size());
  for (const auto& task : config.undistort_tasks) {
    UndistortCacheEntry entry =
        BuildUndistortCacheEntry(config, dataset_root, task);
    const std::size_t entry_index = cache.entries.size();
    cache.entries.push_back(std::move(entry));
    cache.entries_by_camera_id[cache.entries.back().camera_id].push_back(
        entry_index);
  }
  return cache;
}

void SaveUndistortCacheArtifacts(const PipelineConfig& config,
                                 const UndistortCache& cache,
                                 const std::string& output_root) {
  for (const auto& entry : cache.entries) {
    SaveUndistortJsonArtifact(config, entry, output_root);
  }
}

std::vector<UndistortFrameResult> ProcessUndistortFrame(
    const UndistortCache& cache, int camera_id, const GpuImage& image) {
  if (image.image.empty()) {
    throw std::runtime_error("GPU image must not be empty");
  }

  std::vector<UndistortFrameResult> results;
  const auto found = cache.entries_by_camera_id.find(camera_id);
  if (found == cache.entries_by_camera_id.end()) {
    return results;
  }

  results.reserve(found->second.size());
  for (const std::size_t entry_index : found->second) {
    const UndistortCacheEntry& entry = cache.entries.at(entry_index);
    UndistortFrameResult result;
    result.task = entry.task;
    result.image.image = GpuRemap(image.image, entry.gpu_maps);
    results.push_back(std::move(result));
  }
  return results;
}

void SaveUndistortFrameResult(const PipelineConfig& config,
                              const UndistortFrameResult& result,
                              const std::string& output_root,
                              const std::string& input_filename) {
  const std::filesystem::path output_dir =
      std::filesystem::path(output_root) / config.paths.undistort_image_dir_path /
      result.task.image_dir;
  EnsureDirectory(output_dir.string());
  if (result.image.image.empty()) {
    throw std::runtime_error("GPU result image must not be empty");
  }
  const std::string output_path = (output_dir / input_filename).string();
  cv::Mat cpu_image;
  result.image.image.download(cpu_image);
  if (!cv::imwrite(output_path, cpu_image)) {
    throw std::runtime_error("failed to write image: " +
                             output_path);
  }
}

void ValidateUndistortOutputs(const PipelineConfig& config,
                              const std::string& output_root) {
  ValidateUndistortOutputDestinations(config, output_root);
}

void RunUndistortPipeline(const PipelineConfig& config,
                          const std::string& dataset_root,
                          const std::string& output_root) {
  const auto start_time = std::chrono::steady_clock::now();
  LogInfo(config.showinfo != 0, BuildUndistortPipelineStartMessage(config));
  RequireCudaRemapAvailable();

  ValidateUndistortOutputs(config, output_root);

  std::vector<std::chrono::steady_clock::time_point> task_start_times;
  task_start_times.reserve(config.undistort_tasks.size());
  for (const auto& task : config.undistort_tasks) {
    task_start_times.push_back(std::chrono::steady_clock::now());
    LogInfo(config.showinfo != 0, BuildUndistortTaskStartMessage(task));
  }

  const UndistortCache cache = BuildUndistortCache(config, dataset_root);
  SaveUndistortCacheArtifacts(config, cache, output_root);

  const auto source_inputs = BuildSourceCameraInputs(config.undistort_tasks);
  auto process_source_camera =
      [&config, &cache, &dataset_root, &output_root](
          const SourceCameraInput& input) {
    const std::filesystem::path input_dir =
        std::filesystem::path(dataset_root) / config.paths.image_dir_path /
        input.image_dir;
    for (const auto& path : ListFiles(input_dir)) {
      const GpuImage image = ReadImage(path.string());
      const std::vector<UndistortFrameResult> results =
          ProcessUndistortFrame(cache, input.camera_id, image);
      for (const auto& result : results) {
        SaveUndistortFrameResult(config, result, output_root,
                                 path.filename().string());
      }
    }
  };

  if (config.undistort_parallelism <= 1) {
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
    RunJobs(jobs, config.undistort_parallelism);
  }

  for (std::size_t index = 0; index < config.undistort_tasks.size(); ++index) {
    LogInfo(config.showinfo != 0,
            BuildUndistortTaskDoneMessage(
                config.undistort_tasks.at(index),
                ElapsedMilliseconds(task_start_times.at(index))));
  }
  LogInfo(config.showinfo != 0,
          BuildUndistortPipelineDoneMessage(ElapsedMilliseconds(start_time)));
}

}  // namespace vc
