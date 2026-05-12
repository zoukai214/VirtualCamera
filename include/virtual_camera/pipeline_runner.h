#pragma once

#include "virtual_camera/jobs.h"
#include "virtual_camera/pipeline_config.h"

#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace vc {

namespace detail {

template <typename Callback>
inline std::function<void()> MakeContextualPipelineJob(std::string label,
                                                       Callback callback) {
  return [label = std::move(label), callback = std::move(callback)]() mutable {
    try {
      callback();
    } catch (const std::exception& error) {
      throw std::runtime_error(label + ": " + error.what());
    } catch (...) {
      throw std::runtime_error(label + ": unknown exception");
    }
  };
}

}  // namespace detail

template <typename UndistortCallback, typename VirtualCameraCallback>
inline void RunTopLevelPipelines(const PipelineConfig& config,
                                 UndistortCallback undistort_callback,
                                 VirtualCameraCallback virtual_camera_callback) {
  std::vector<std::function<void()>> pipeline_jobs;
  if (config.process_undistort != 0) {
    pipeline_jobs.push_back(detail::MakeContextualPipelineJob(
        "undistort pipeline", std::move(undistort_callback)));
  }
  if (config.process_virtual_camera != 0) {
    pipeline_jobs.push_back(detail::MakeContextualPipelineJob(
        "virtual_camera pipeline", std::move(virtual_camera_callback)));
  }

  RunJobs(pipeline_jobs, config.task_parallelism);
}

}  // namespace vc
