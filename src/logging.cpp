#include "virtual_camera/logging.h"

#include <iostream>
#include <mutex>
#include <sstream>

namespace vc {

namespace {

std::mutex& LogMutex() {
  static std::mutex mutex;
  return mutex;
}

std::string BoolText(bool value) {
  return value ? "true" : "false";
}

}  // namespace

void LogInfo(bool enabled, const std::string& message) {
  if (!enabled) {
    return;
  }
  std::lock_guard<std::mutex> lock(LogMutex());
  std::cout << "[INFO] " << message << "\n";
}

std::string BuildRt024PipelineStartMessage(const Rt024RuntimeArgs& args,
                                           const PipelineConfig& config) {
  static_cast<void>(config);
  std::ostringstream stream;
  stream << "pipeline start: dataset=" << args.dataset_root
         << " config=" << args.config_path << " output=" << args.output_root
         << " debug=" << BoolText(args.debug);
  return stream.str();
}

std::string BuildUndistortPipelineStartMessage(const PipelineConfig& config) {
  std::ostringstream stream;
  stream << "undistort start: tasks=" << config.undistort_tasks.size()
         << " parallelism=" << config.undistort_parallelism;
  return stream.str();
}

std::string BuildUndistortPipelineDoneMessage(long long elapsed_ms) {
  std::ostringstream stream;
  stream << "undistort done: elapsed_ms=" << elapsed_ms;
  return stream.str();
}

std::string BuildUndistortTaskStartMessage(const UndistortTaskConfig& task) {
  std::ostringstream stream;
  stream << "undistort task start: image_dir=" << task.image_dir
         << " calib=" << task.conf_json;
  return stream.str();
}

std::string BuildUndistortTaskDoneMessage(const UndistortTaskConfig& task,
                                          long long elapsed_ms) {
  std::ostringstream stream;
  stream << "undistort task done: image_dir=" << task.image_dir
         << " elapsed_ms=" << elapsed_ms;
  return stream.str();
}

std::string BuildVirtualCameraPipelineStartMessage(
    const PipelineConfig& config) {
  std::ostringstream stream;
  stream << "virtual_camera start: tasks=" << config.virtual_tasks.size()
         << " parallelism=" << config.virtual_camera_parallelism;
  return stream.str();
}

std::string BuildVirtualCameraPipelineDoneMessage(long long elapsed_ms) {
  std::ostringstream stream;
  stream << "virtual_camera done: elapsed_ms=" << elapsed_ms;
  return stream.str();
}

std::string BuildVirtualTaskStartMessage(const VirtualCameraTaskConfig& task) {
  std::ostringstream stream;
  stream << "virtual task start: prefix=" << task.file_prefix
         << " save_dir=" << task.save_dir << " calib=" << task.calib_json;
  return stream.str();
}

std::string BuildVirtualTaskDoneMessage(const VirtualCameraTaskConfig& task,
                                        long long elapsed_ms) {
  std::ostringstream stream;
  stream << "virtual task done: prefix=" << task.file_prefix
         << " save_dir=" << task.save_dir << " elapsed_ms=" << elapsed_ms;
  return stream.str();
}

}  // namespace vc
