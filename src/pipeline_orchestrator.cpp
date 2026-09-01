#include "virtual_camera/pipeline_orchestrator.h"

#include <unordered_set>

namespace vc {
namespace {

std::vector<SourceCameraInput> BuildUndistortSourceInputs(
    const UndistortCache& cache) {
  std::vector<SourceCameraInput> inputs;
  std::unordered_set<int> seen_camera_ids;
  for (const auto& entry : cache.entries) {
    if (!seen_camera_ids.insert(entry.camera_id).second) {
      continue;
    }
    inputs.push_back(SourceCameraInput{entry.camera_id, entry.task.image_dir});
  }
  return inputs;
}

std::vector<SourceCameraInput> BuildVirtualCameraSourceInputs(
    const VirtualCameraCache& cache) {
  std::vector<SourceCameraInput> inputs;
  std::unordered_set<int> seen_camera_ids;
  for (const auto& entry : cache.entries) {
    if (!seen_camera_ids.insert(entry.task.camera_id).second) {
      continue;
    }
    inputs.push_back(
        SourceCameraInput{entry.task.camera_id, entry.task.image_dir});
  }
  return inputs;
}

}  // namespace

PipelineOrchestrator::PipelineOrchestrator(const std::string& config_path,
                                           const std::string& dataset_root,
                                           PipelineSelection selection)
    : config_(LoadPipelineConfig(config_path)) {
  const bool build_undistort =
      selection == PipelineSelection::kAll ||
      selection == PipelineSelection::kUndistort;
  const bool build_virtual_camera =
      selection == PipelineSelection::kAll ||
      selection == PipelineSelection::kVirtualCamera;

  if (build_undistort && config_.process_undistort != 0) {
    undistort_cache_ = BuildUndistortCache(config_, dataset_root);
    undistort_source_inputs_ = BuildUndistortSourceInputs(undistort_cache_);
  }
  if (build_virtual_camera && config_.process_virtual_camera != 0) {
    virtual_camera_cache_ = BuildVirtualCameraCache(config_, dataset_root);
    virtual_source_inputs_ =
        BuildVirtualCameraSourceInputs(virtual_camera_cache_);
  }
}

const std::vector<SourceCameraInput>&
PipelineOrchestrator::UndistortSourceInputs() const {
  return undistort_source_inputs_;
}

const std::vector<SourceCameraInput>&
PipelineOrchestrator::VirtualSourceInputs() const {
  return virtual_source_inputs_;
}

void PipelineOrchestrator::SaveUndistortArtifacts(
    const std::string& output_root) const {
  ValidateUndistortOutputs(config_, output_root);
  SaveUndistortCacheArtifacts(config_, undistort_cache_, output_root);
}

void PipelineOrchestrator::SaveVirtualCameraArtifacts(
    const std::string& output_root) const {
  ValidateVirtualCameraOutputs(config_, output_root);
  SaveVirtualCameraCacheArtifacts(config_, virtual_camera_cache_, output_root);
}

GpuImage PipelineOrchestrator::ReadImage(const std::string& image_path) const {
  return vc::ReadImage(image_path);
}

std::vector<UndistortFrameResult> PipelineOrchestrator::ProcessUndistortFrame(
    int camera_id, const GpuImage& image) const {
  return vc::ProcessUndistortFrame(undistort_cache_, camera_id, image);
}

std::vector<VirtualCameraFrameResult>
PipelineOrchestrator::ProcessVirtualCameraFrame(
    int camera_id, const GpuImage& image) const {
  return vc::ProcessVirtualCameraFrame(virtual_camera_cache_, camera_id, image);
}

void PipelineOrchestrator::SaveUndistortFrameResults(
    const std::vector<UndistortFrameResult>& results,
    const std::string& output_root, const std::string& input_filename) const {
  for (const auto& result : results) {
    SaveUndistortFrameResult(config_, result, output_root, input_filename);
  }
}

void PipelineOrchestrator::SaveVirtualCameraFrameResults(
    const std::vector<VirtualCameraFrameResult>& results,
    const std::string& output_root, const std::string& input_filename) const {
  for (const auto& result : results) {
    SaveVirtualCameraFrameResult(config_, result, output_root, input_filename);
  }
}

}  // namespace vc
