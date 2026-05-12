#include "virtual_camera/pipeline_config.h"
#include "virtual_camera/pipeline_runner.h"

#include <atomic>
#include <functional>
#include <stdexcept>
#include <string>

namespace {

void Expect(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

vc::PipelineConfig MakeConfig(int process_undistort, int process_virtual_camera,
                              int task_parallelism) {
  vc::PipelineConfig config;
  config.process_undistort = process_undistort;
  config.process_virtual_camera = process_virtual_camera;
  config.task_parallelism = task_parallelism;
  return config;
}

void TestRunTopLevelPipelinesRunsBothEnabledCallbacks() {
  std::atomic<int> undistort_count{0};
  std::atomic<int> virtual_camera_count{0};
  const vc::PipelineConfig config = MakeConfig(1, 1, 2);

  vc::RunTopLevelPipelines(
      config,
      [&undistort_count]() { ++undistort_count; },
      [&virtual_camera_count]() { ++virtual_camera_count; });

  Expect(undistort_count.load() == 1, "undistort callback should run");
  Expect(virtual_camera_count.load() == 1,
         "virtual camera callback should run");
}

void TestRunTopLevelPipelinesSkipsDisabledCallback() {
  std::atomic<int> undistort_count{0};
  std::atomic<int> virtual_camera_count{0};
  const vc::PipelineConfig config = MakeConfig(0, 1, 2);

  vc::RunTopLevelPipelines(
      config,
      [&undistort_count]() { ++undistort_count; },
      [&virtual_camera_count]() { ++virtual_camera_count; });

  Expect(undistort_count.load() == 0, "disabled callback should not run");
  Expect(virtual_camera_count.load() == 1,
         "enabled callback should run once");
}

void TestRunTopLevelPipelinesWrapsPipelineFailure() {
  const vc::PipelineConfig config = MakeConfig(1, 0, 2);

  bool thrown = false;
  try {
    vc::RunTopLevelPipelines(
        config,
        []() { throw std::runtime_error("boom"); },
        []() {});
  } catch (const std::runtime_error& error) {
    const std::string message = error.what();
    thrown = message.find("undistort pipeline") != std::string::npos &&
             message.find("boom") != std::string::npos;
  }
  Expect(thrown, "pipeline failure should include pipeline label");
}

}  // namespace

int main() {
  TestRunTopLevelPipelinesRunsBothEnabledCallbacks();
  TestRunTopLevelPipelinesSkipsDisabledCallback();
  TestRunTopLevelPipelinesWrapsPipelineFailure();
  return 0;
}
