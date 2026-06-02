#include "virtual_camera/logging.h"

#include "virtual_camera/pipeline_config.h"
#include "virtual_camera/runtime_args.h"

#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

void Expect(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

std::string CaptureStdout(const std::function<void()>& callback) {
  std::ostringstream stream;
  std::streambuf* const original = std::cout.rdbuf(stream.rdbuf());
  try {
    callback();
  } catch (...) {
    std::cout.rdbuf(original);
    throw;
  }
  std::cout.rdbuf(original);
  return stream.str();
}

void TestLogInfoPrintsSinglePrefixedLine() {
  const std::string output = CaptureStdout([]() {
    vc::LogInfo(true, "undistort start: tasks=7 parallelism=7");
  });
  Expect(output == "[INFO] undistort start: tasks=7 parallelism=7\n",
         "LogInfo should print one prefixed line");
}

void TestLogInfoSkipsDisabledMessages() {
  const std::string output =
      CaptureStdout([]() { vc::LogInfo(false, "should not print"); });
  Expect(output.empty(), "LogInfo should skip disabled messages");
}

void TestBuildRt024PipelineStartMessageIncludesRuntimeRoots() {
  vc::Rt024RuntimeArgs args;
  args.dataset_root = "/tmp/dataset";
  args.config_path = "configs/config_rt024_parallel_run.json";
  args.output_root = "/tmp/output";
  args.debug = true;

  vc::PipelineConfig config;

  const std::string message =
      vc::BuildRt024PipelineStartMessage(args, config);
  Expect(message.find("pipeline start:") != std::string::npos,
         "message should contain pipeline prefix");
  Expect(message.find("dataset=/tmp/dataset") != std::string::npos,
         "message should contain dataset root");
  Expect(
      message.find("config=configs/config_rt024_parallel_run.json") !=
          std::string::npos,
      "message should contain config path");
  Expect(message.find("output=/tmp/output") != std::string::npos,
         "message should contain output root");
  Expect(message.find("debug=true") != std::string::npos,
         "message should contain debug flag");
}

void TestBuildVirtualTaskStartMessageIncludesTaskIdentity() {
  vc::VirtualCameraTaskConfig task;
  task.file_prefix = "fw110";
  task.save_dir = "front_wide_110/";
  task.calib_json = "calib_cam_front_wide_fov110.json";

  const std::string message = vc::BuildVirtualTaskStartMessage(task);
  Expect(message.find("virtual task start:") != std::string::npos,
         "message should contain task prefix");
  Expect(message.find("prefix=fw110") != std::string::npos,
         "message should contain file prefix");
  Expect(message.find("save_dir=front_wide_110/") != std::string::npos,
         "message should contain save_dir");
  Expect(message.find("calib=calib_cam_front_wide_fov110.json") !=
             std::string::npos,
         "message should contain calib json");
}

}  // namespace

int main() {
  TestLogInfoPrintsSinglePrefixedLine();
  TestLogInfoSkipsDisabledMessages();
  TestBuildRt024PipelineStartMessageIncludesRuntimeRoots();
  TestBuildVirtualTaskStartMessageIncludesTaskIdentity();
  return 0;
}
