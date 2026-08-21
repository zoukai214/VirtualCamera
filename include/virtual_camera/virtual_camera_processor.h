#pragma once

#include "virtual_camera/cuda_remap.h"
#include "virtual_camera/pipeline_config.h"
#include "virtual_camera/remap_generator.h"

#include <Eigen/Dense>
#include <opencv2/core.hpp>

#include <unordered_map>
#include <vector>

namespace vc {

struct VirtualCameraCacheEntry {
  VirtualCameraTaskConfig task;
  VirtualCameraMaps maps;
  CudaRemapMaps gpu_maps;
  Eigen::Matrix3d virtual_intrinsic = Eigen::Matrix3d::Identity();
  Eigen::Matrix4d virtual_extrinsic = Eigen::Matrix4d::Identity();
  std::vector<double> dist_data;
};

struct VirtualCameraCache {
  std::vector<VirtualCameraCacheEntry> entries;
  std::unordered_map<int, std::vector<std::size_t>> entries_by_camera_id;
};

struct VirtualCameraFrameResult {
  VirtualCameraTaskConfig task;
  cv::Mat image;
};

VirtualCameraCache BuildVirtualCameraCache(const PipelineConfig& config,
                                           const std::string& dataset_root);
void SaveVirtualCameraCacheArtifacts(const PipelineConfig& config,
                                     const VirtualCameraCache& cache,
                                     const std::string& output_root);
std::vector<VirtualCameraFrameResult> ProcessVirtualCameraFrame(
    const VirtualCameraCache& cache, int camera_id, const cv::Mat& image);
void SaveVirtualCameraFrameResult(const PipelineConfig& config,
                                  const VirtualCameraFrameResult& result,
                                  const std::string& output_root,
                                  const std::string& input_filename);
void ValidateVirtualCameraOutputs(const PipelineConfig& config,
                                  const std::string& output_root);
void RunVirtualCameraPipeline(const PipelineConfig& config,
                              const std::string& dataset_root,
                              const std::string& output_root);

}  // namespace vc
