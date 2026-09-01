#include "virtual_camera/pipeline_config.h"
#include "virtual_camera/undistort_processor.h"
#include "virtual_camera/virtual_camera_processor.h"

#include <opencv2/core/utility.hpp>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

struct RuntimeArgs {
  std::string dataset_root;
  std::string config_path;
  std::string output_root;
};

struct SourceCameraInput {
  int camera_id = 0;
  std::string image_dir;
};

struct StageTiming {
  double total_ms = 0.0;
  std::size_t source_image_count = 0;
  std::size_t output_image_count = 0;
};

class ScopedTimer {
 public:
  explicit ScopedTimer(double* elapsed_ms)
      : elapsed_ms_(elapsed_ms), start_time_(Clock::now()) {}

  ~ScopedTimer() {
    *elapsed_ms_ += std::chrono::duration<double, std::milli>(
                        Clock::now() - start_time_)
                        .count();
  }

 private:
  double* elapsed_ms_;
  Clock::time_point start_time_;
};

std::string BuildUsage(const std::string& program_name) {
  return "Usage: " + program_name +
         " --dataset_root <path> --config_path <path> --output_root <path>";
}

const char* RequireValue(int argc, char** argv, int index,
                         const std::string& flag) {
  if (index + 1 >= argc) {
    throw std::runtime_error("missing value for " + flag);
  }
  const char* value = argv[index + 1];
  if (value != nullptr && std::string(value).rfind("--", 0) == 0) {
    throw std::runtime_error("missing value for " + flag);
  }
  return value;
}

RuntimeArgs ParseArgs(int argc, char** argv) {
  if (argc <= 0 || argv == nullptr || argv[0] == nullptr) {
    throw std::runtime_error("invalid argv");
  }

  RuntimeArgs args;
  for (int index = 1; index < argc; ++index) {
    const std::string flag = argv[index];
    if (flag == "--dataset_root") {
      args.dataset_root = RequireValue(argc, argv, index, flag);
      ++index;
      continue;
    }
    if (flag == "--config_path") {
      args.config_path = RequireValue(argc, argv, index, flag);
      ++index;
      continue;
    }
    if (flag == "--output_root") {
      args.output_root = RequireValue(argc, argv, index, flag);
      ++index;
      continue;
    }
    throw std::runtime_error("unknown argument: " + flag);
  }

  if (args.dataset_root.empty()) {
    throw std::runtime_error("missing required flag --dataset_root");
  }
  if (args.config_path.empty()) {
    throw std::runtime_error("missing required flag --config_path");
  }
  if (args.output_root.empty()) {
    throw std::runtime_error("missing required flag --output_root");
  }
  return args;
}

std::vector<std::filesystem::path> ListFiles(const std::filesystem::path& dir) {
  std::vector<std::filesystem::path> files;
  for (const auto& entry : std::filesystem::directory_iterator(dir)) {
    if (entry.is_regular_file()) {
      files.push_back(entry.path());
    }
  }
  std::sort(files.begin(), files.end());
  return files;
}

vc::GpuImage ReadFrame(const std::filesystem::path& path) {
  return vc::ReadImage(path.string());
}

std::vector<SourceCameraInput> BuildUndistortSourceInputs(
    const vc::UndistortCache& cache) {
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
    const vc::VirtualCameraCache& cache) {
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

void BenchmarkUndistortFrames(
    const vc::PipelineConfig& config, const vc::UndistortCache& cache,
    const std::string& dataset_root, const std::string& output_root,
    StageTiming* read_timing, StageTiming* process_timing,
    StageTiming* save_timing) {
  for (const auto& source_camera : BuildUndistortSourceInputs(cache)) {
    const std::filesystem::path input_dir =
        std::filesystem::path(dataset_root) / config.paths.image_dir_path /
        source_camera.image_dir;
    for (const auto& image_path : ListFiles(input_dir)) {
      vc::GpuImage image;
      {
        ScopedTimer timer(&read_timing->total_ms);
        image = ReadFrame(image_path);
      }
      ++read_timing->source_image_count;

      std::vector<vc::UndistortFrameResult> results;
      {
        ScopedTimer timer(&process_timing->total_ms);
        results = vc::ProcessUndistortFrame(cache, source_camera.camera_id,
                                            image);
      }
      ++process_timing->source_image_count;
      process_timing->output_image_count += results.size();

      {
        ScopedTimer timer(&save_timing->total_ms);
        for (const auto& result : results) {
          vc::SaveUndistortFrameResult(config, result, output_root,
                                       image_path.filename().string());
        }
      }
      save_timing->source_image_count += results.empty() ? 0 : 1;
      save_timing->output_image_count += results.size();
    }
  }
}

void BenchmarkVirtualCameraFrames(
    const vc::PipelineConfig& config, const vc::VirtualCameraCache& cache,
    const std::string& dataset_root, const std::string& output_root,
    StageTiming* read_timing, StageTiming* process_timing,
    StageTiming* save_timing) {
  for (const auto& source_camera : BuildVirtualCameraSourceInputs(cache)) {
    const std::filesystem::path input_dir =
        std::filesystem::path(dataset_root) / config.paths.image_dir_path /
        source_camera.image_dir;
    for (const auto& image_path : ListFiles(input_dir)) {
      vc::GpuImage image;
      {
        ScopedTimer timer(&read_timing->total_ms);
        image = ReadFrame(image_path);
      }
      ++read_timing->source_image_count;

      std::vector<vc::VirtualCameraFrameResult> results;
      {
        ScopedTimer timer(&process_timing->total_ms);
        results = vc::ProcessVirtualCameraFrame(cache, source_camera.camera_id,
                                                image);
      }
      ++process_timing->source_image_count;
      process_timing->output_image_count += results.size();

      {
        ScopedTimer timer(&save_timing->total_ms);
        for (const auto& result : results) {
          vc::SaveVirtualCameraFrameResult(config, result, output_root,
                                           image_path.filename().string());
        }
      }
      save_timing->source_image_count += results.empty() ? 0 : 1;
      save_timing->output_image_count += results.size();
    }
  }
}

double AverageMs(double total_ms, std::size_t count) {
  if (count == 0) {
    return 0.0;
  }
  return total_ms / static_cast<double>(count);
}

void PrintStage(const std::string& name, const StageTiming& timing) {
  std::cout << "| " << name << " | " << timing.source_image_count << " | "
            << timing.output_image_count << " | " << std::fixed
            << std::setprecision(3) << timing.total_ms << " | "
            << AverageMs(timing.total_ms, timing.source_image_count) << " | "
            << AverageMs(timing.total_ms, timing.output_image_count) << " |\n";
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const RuntimeArgs args = ParseArgs(argc, argv);
    cv::setNumThreads(1);

    double config_and_map_ms = 0.0;
    vc::PipelineConfig config;
    vc::UndistortCache undistort_cache;
    vc::VirtualCameraCache virtual_camera_cache;
    {
      ScopedTimer timer(&config_and_map_ms);
      config = vc::LoadPipelineConfig(args.config_path);
      undistort_cache = vc::BuildUndistortCache(config, args.dataset_root);
      virtual_camera_cache =
          vc::BuildVirtualCameraCache(config, args.dataset_root);
    }

    StageTiming undistort_read;
    StageTiming undistort_process;
    StageTiming undistort_save;
    StageTiming virtual_read;
    StageTiming virtual_process;
    StageTiming virtual_save;

    BenchmarkUndistortFrames(config, undistort_cache, args.dataset_root,
                             args.output_root, &undistort_read,
                             &undistort_process, &undistort_save);
    BenchmarkVirtualCameraFrames(config, virtual_camera_cache,
                                 args.dataset_root, args.output_root,
                                 &virtual_read, &virtual_process,
                                 &virtual_save);

    std::cout << "dataset_root: " << args.dataset_root << "\n";
    std::cout << "config_path: " << args.config_path << "\n";
    std::cout << "output_root: " << args.output_root << "\n";
    std::cout << "opencv_threads: " << cv::getNumThreads() << "\n";
    std::cout << "config_and_map_ms: " << std::fixed << std::setprecision(3)
              << config_and_map_ms << "\n";
    std::cout << "| stage | source_images | output_images | total_ms | "
                 "avg_ms_per_source_image | avg_ms_per_output_image |\n";
    std::cout << "| --- | ---: | ---: | ---: | ---: | ---: |\n";
    PrintStage("undistort_read", undistort_read);
    PrintStage("undistort_process", undistort_process);
    PrintStage("undistort_save", undistort_save);
    PrintStage("virtual_camera_read", virtual_read);
    PrintStage("virtual_camera_process", virtual_process);
    PrintStage("virtual_camera_save", virtual_save);
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << "\n";
    if (argc > 0 && argv != nullptr && argv[0] != nullptr) {
      std::cerr << BuildUsage(argv[0]) << "\n";
    }
    return 1;
  }
}
