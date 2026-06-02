#include "virtual_camera/logging.h"

#include "virtual_camera/pipeline_config.h"
#include "virtual_camera/runtime_args.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

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

std::vector<std::string> SplitLines(const std::string& text) {
  std::vector<std::string> lines;
  std::istringstream stream(text);
  std::string line;
  while (std::getline(stream, line)) {
    lines.push_back(line);
  }
  return lines;
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
  args.output_root = "/tmp/args-output";
  args.debug = true;

  vc::PipelineConfig config;
  config.output_root = "/tmp/output";

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

void TestBuildUndistortPipelineStartMessageIncludesTaskCountAndParallelism() {
  vc::PipelineConfig config;
  config.undistort_parallelism = 3;
  config.undistort_tasks.resize(2);

  const std::string message = vc::BuildUndistortPipelineStartMessage(config);
  Expect(message == "undistort start: tasks=2 parallelism=3",
         "undistort start message should match expected format");
}

void TestBuildUndistortPipelineDoneMessageIncludesElapsedMilliseconds() {
  const std::string message = vc::BuildUndistortPipelineDoneMessage(42);
  Expect(message == "undistort done: elapsed_ms=42",
         "undistort done message should match expected format");
}

void TestBuildVirtualTaskDoneMessageIncludesTaskIdentityAndElapsedMilliseconds() {
  vc::VirtualCameraTaskConfig task;
  task.file_prefix = "fw110";
  task.save_dir = "front_wide_110/";

  const std::string message = vc::BuildVirtualTaskDoneMessage(task, 15);
  Expect(
      message ==
          "virtual task done: prefix=fw110 save_dir=front_wide_110/ elapsed_ms=15",
      "virtual task done message should match expected format");
}

void TestLogInfoKeepsConcurrentLinesIntact() {
  const std::vector<std::string> messages = {
      "thread-0 message", "thread-1 message", "thread-2 message",
      "thread-3 message"};
  const std::string output = CaptureStdout([&messages]() {
    std::vector<std::thread> threads;
    threads.reserve(messages.size());
    for (const std::string& message : messages) {
      threads.emplace_back([message]() { vc::LogInfo(true, message); });
    }
    for (std::thread& thread : threads) {
      thread.join();
    }
  });

  std::vector<std::string> lines = SplitLines(output);
  Expect(lines.size() == messages.size(),
         "concurrent LogInfo should produce one complete line per message");

  std::vector<std::string> expected_lines;
  expected_lines.reserve(messages.size());
  for (const std::string& message : messages) {
    expected_lines.push_back("[INFO] " + message);
  }
  std::sort(lines.begin(), lines.end());
  std::sort(expected_lines.begin(), expected_lines.end());
  Expect(lines == expected_lines,
         "concurrent LogInfo output should contain intact expected lines");
}

}  // namespace

int main() {
  TestLogInfoPrintsSinglePrefixedLine();
  TestLogInfoSkipsDisabledMessages();
  TestLogInfoKeepsConcurrentLinesIntact();
  TestBuildRt024PipelineStartMessageIncludesRuntimeRoots();
  TestBuildUndistortPipelineStartMessageIncludesTaskCountAndParallelism();
  TestBuildUndistortPipelineDoneMessageIncludesElapsedMilliseconds();
  TestBuildVirtualTaskStartMessageIncludesTaskIdentity();
  TestBuildVirtualTaskDoneMessageIncludesTaskIdentityAndElapsedMilliseconds();
  return 0;
}
