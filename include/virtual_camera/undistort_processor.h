#pragma once

#include "virtual_camera/pipeline_config.h"
#include "virtual_camera/cuda_remap.h"
#include "virtual_camera/remap_generator.h"
#include "virtual_camera/types.h"

#include <opencv2/core.hpp>

#include <unordered_map>
#include <vector>

namespace vc {

struct UndistortCacheEntry {
  UndistortTaskConfig task;
  int camera_id = 0;
  CalibrationParam calibration;
  UndistortMaps maps;
  CudaRemapMaps gpu_maps;
};

struct UndistortCache {
  std::vector<UndistortCacheEntry> entries;
  std::unordered_map<int, std::vector<std::size_t>> entries_by_camera_id;
};

struct UndistortFrameResult {
  UndistortTaskConfig task;
  cv::Mat image;
};

UndistortCache BuildUndistortCache(const PipelineConfig& config,
                                   const std::string& dataset_root);
void SaveUndistortCacheArtifacts(const PipelineConfig& config,
                                 const UndistortCache& cache,
                                 const std::string& output_root);
std::vector<UndistortFrameResult> ProcessUndistortFrame(
    const UndistortCache& cache, int camera_id, const cv::Mat& image);
void SaveUndistortFrameResult(const PipelineConfig& config,
                              const UndistortFrameResult& result,
                              const std::string& output_root,
                              const std::string& input_filename);
void ValidateUndistortOutputs(const PipelineConfig& config,
                              const std::string& output_root);
void RunUndistortPipeline(const PipelineConfig& config,
                          const std::string& dataset_root,
                          const std::string& output_root);

}  // namespace vc
