#pragma once

#include "virtual_camera/pipeline_config.h"
#include "virtual_camera/runtime_args.h"

#include <chrono>
#include <string>

namespace vc {

void LogInfo(bool enabled, const std::string& message);

std::string BuildRt024PipelineStartMessage(const Rt024RuntimeArgs& args,
                                           const PipelineConfig& config);
std::string BuildUndistortPipelineStartMessage(const PipelineConfig& config);
std::string BuildUndistortPipelineDoneMessage(long long elapsed_ms);
std::string BuildUndistortTaskStartMessage(const UndistortTaskConfig& task);
std::string BuildUndistortTaskDoneMessage(const UndistortTaskConfig& task,
                                          long long elapsed_ms);
std::string BuildVirtualCameraPipelineStartMessage(const PipelineConfig& config);
std::string BuildVirtualCameraPipelineDoneMessage(long long elapsed_ms);
std::string BuildVirtualTaskStartMessage(const VirtualCameraTaskConfig& task);
std::string BuildVirtualTaskDoneMessage(const VirtualCameraTaskConfig& task,
                                        long long elapsed_ms);

inline long long ElapsedMilliseconds(
    const std::chrono::steady_clock::time_point& start_time) {
  const auto now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time)
      .count();
}

}  // namespace vc
