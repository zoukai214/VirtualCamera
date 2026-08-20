#pragma once

#include "virtual_camera/pipeline_config.h"
#include "virtual_camera/undistort_processor.h"
#include "virtual_camera/virtual_camera_processor.h"

#include <opencv2/core.hpp>

#include <string>
#include <vector>

namespace vc {

struct SourceCameraInput {
  int camera_id = 0;
  std::string image_dir;
};

enum class PipelineSelection {
  kAll,
  kUndistort,
  kVirtualCamera,
};

class PipelineOrchestrator {
 public:
  explicit PipelineOrchestrator(
      const std::string& config_path, const std::string& dataset_root,
      PipelineSelection selection = PipelineSelection::kAll);

  const std::vector<SourceCameraInput>& UndistortSourceInputs() const;
  const std::vector<SourceCameraInput>& VirtualSourceInputs() const;

  void SaveUndistortArtifacts(const std::string& output_root) const;
  void SaveVirtualCameraArtifacts(const std::string& output_root) const;

  void ProcessUndistortFrame(int camera_id, const cv::Mat& image,
                             const std::string& output_root,
                             const std::string& input_filename) const;
  void ProcessVirtualCameraFrame(int camera_id, const cv::Mat& image,
                                 const std::string& output_root,
                                 const std::string& input_filename) const;

 private:
  PipelineConfig config_;
  UndistortCache undistort_cache_;
  VirtualCameraCache virtual_camera_cache_;
  std::vector<SourceCameraInput> undistort_source_inputs_;
  std::vector<SourceCameraInput> virtual_source_inputs_;
};

}  // namespace vc
