#include "virtual_camera/jobs.h"
#include "virtual_camera/pipeline_config.h"
#include "virtual_camera/undistort_processor.h"
#include "virtual_camera/virtual_camera_processor.h"

#include <opencv2/core/utility.hpp>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
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

struct PipelineTiming {
  StageTiming read;
  StageTiming process;
  StageTiming save;
  double elapsed_ms = 0.0;
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

class PipelineTimingAccumulator {
 public:
  void AddRead(double total_ms, std::size_t source_images) {
    std::lock_guard<std::mutex> lock(mutex_);
    timing_.read.total_ms += total_ms;
    timing_.read.source_image_count += source_images;
  }

  void AddProcess(double total_ms, std::size_t source_images,
                  std::size_t output_images) {
    std::lock_guard<std::mutex> lock(mutex_);
    timing_.process.total_ms += total_ms;
    timing_.process.source_image_count += source_images;
    timing_.process.output_image_count += output_images;
  }

  void AddSave(double total_ms, std::size_t source_images,
               std::size_t output_images) {
    std::lock_guard<std::mutex> lock(mutex_);
    timing_.save.total_ms += total_ms;
    timing_.save.source_image_count += source_images;
    timing_.save.output_image_count += output_images;
  }

  PipelineTiming Snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return timing_;
  }

 private:
  mutable std::mutex mutex_;
  PipelineTiming timing_;
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

void RunUndistortBenchmark(const vc::PipelineConfig& config,
                           const vc::UndistortCache& cache,
                           const std::string& dataset_root,
                           const std::string& output_root,
                           PipelineTiming* timing) {
  const auto pipeline_start = Clock::now();
  vc::ValidateUndistortOutputs(config, output_root);
  vc::SaveUndistortCacheArtifacts(config, cache, output_root);

  PipelineTimingAccumulator accumulator;
  const std::vector<SourceCameraInput> source_inputs =
      BuildUndistortSourceInputs(cache);
  std::vector<std::function<void()>> jobs;
  jobs.reserve(source_inputs.size());
  for (const auto& source_camera : source_inputs) {
    jobs.push_back([&config, &cache, &dataset_root, &output_root,
                    &accumulator, source_camera]() {
      const std::filesystem::path input_dir =
          std::filesystem::path(dataset_root) / config.paths.image_dir_path /
          source_camera.image_dir;
      for (const auto& image_path : ListFiles(input_dir)) {
        vc::GpuImage image;
        double read_ms = 0.0;
        {
          ScopedTimer timer(&read_ms);
          image = ReadFrame(image_path);
        }
        accumulator.AddRead(read_ms, 1);

        std::vector<vc::UndistortFrameResult> results;
        double process_ms = 0.0;
        {
          ScopedTimer timer(&process_ms);
          results =
              vc::ProcessUndistortFrame(cache, source_camera.camera_id, image);
        }
        accumulator.AddProcess(process_ms, 1, results.size());

        double save_ms = 0.0;
        {
          ScopedTimer timer(&save_ms);
          for (const auto& result : results) {
            vc::SaveUndistortFrameResult(config, result, output_root,
                                         image_path.filename().string());
          }
        }
        accumulator.AddSave(save_ms, results.empty() ? 0 : 1, results.size());
      }
    });
  }
  vc::RunJobs(jobs, config.undistort_parallelism);
  *timing = accumulator.Snapshot();
  timing->elapsed_ms =
      std::chrono::duration<double, std::milli>(Clock::now() - pipeline_start)
          .count();
}

void RunVirtualCameraBenchmark(const vc::PipelineConfig& config,
                               const vc::VirtualCameraCache& cache,
                               const std::string& dataset_root,
                               const std::string& output_root,
                               PipelineTiming* timing) {
  const auto pipeline_start = Clock::now();
  vc::ValidateVirtualCameraOutputs(config, output_root);
  vc::SaveVirtualCameraCacheArtifacts(config, cache, output_root);

  PipelineTimingAccumulator accumulator;
  const std::vector<SourceCameraInput> source_inputs =
      BuildVirtualCameraSourceInputs(cache);
  std::vector<std::function<void()>> jobs;
  jobs.reserve(source_inputs.size());
  for (const auto& source_camera : source_inputs) {
    jobs.push_back([&config, &cache, &dataset_root, &output_root,
                    &accumulator, source_camera]() {
      const std::filesystem::path input_dir =
          std::filesystem::path(dataset_root) / config.paths.image_dir_path /
          source_camera.image_dir;
      for (const auto& image_path : ListFiles(input_dir)) {
        vc::GpuImage image;
        double read_ms = 0.0;
        {
          ScopedTimer timer(&read_ms);
          image = ReadFrame(image_path);
        }
        accumulator.AddRead(read_ms, 1);

        std::vector<vc::VirtualCameraFrameResult> results;
        double process_ms = 0.0;
        {
          ScopedTimer timer(&process_ms);
          results = vc::ProcessVirtualCameraFrame(
              cache, source_camera.camera_id, image);
        }
        accumulator.AddProcess(process_ms, 1, results.size());

        double save_ms = 0.0;
        {
          ScopedTimer timer(&save_ms);
          for (const auto& result : results) {
            vc::SaveVirtualCameraFrameResult(config, result, output_root,
                                             image_path.filename().string());
          }
        }
        accumulator.AddSave(save_ms, results.empty() ? 0 : 1, results.size());
      }
    });
  }
  vc::RunJobs(jobs, config.virtual_camera_parallelism);
  *timing = accumulator.Snapshot();
  timing->elapsed_ms =
      std::chrono::duration<double, std::milli>(Clock::now() - pipeline_start)
          .count();
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

    PipelineTiming undistort_timing;
    PipelineTiming virtual_camera_timing;
    const auto total_start = Clock::now();
    std::vector<std::function<void()>> pipeline_jobs;
    if (config.process_undistort != 0) {
      pipeline_jobs.push_back([&]() {
        RunUndistortBenchmark(config, undistort_cache, args.dataset_root,
                              args.output_root, &undistort_timing);
      });
    }
    if (config.process_virtual_camera != 0) {
      pipeline_jobs.push_back([&]() {
        RunVirtualCameraBenchmark(config, virtual_camera_cache,
                                  args.dataset_root, args.output_root,
                                  &virtual_camera_timing);
      });
    }
    vc::RunJobs(pipeline_jobs, config.task_parallelism);
    const double total_elapsed_ms =
        std::chrono::duration<double, std::milli>(Clock::now() - total_start)
            .count();

    std::cout << "dataset_root: " << args.dataset_root << "\n";
    std::cout << "config_path: " << args.config_path << "\n";
    std::cout << "output_root: " << args.output_root << "\n";
    std::cout << "opencv_threads: " << cv::getNumThreads() << "\n";
    std::cout << "task_parallelism: " << config.task_parallelism << "\n";
    std::cout << "undistort_parallelism: " << config.undistort_parallelism
              << "\n";
    std::cout << "virtual_camera_parallelism: "
              << config.virtual_camera_parallelism << "\n";
    std::cout << "config_and_map_ms: " << std::fixed << std::setprecision(3)
              << config_and_map_ms << "\n";
    std::cout << "parallel_total_elapsed_ms: " << total_elapsed_ms << "\n";
    std::cout << "undistort_elapsed_ms: " << undistort_timing.elapsed_ms
              << "\n";
    std::cout << "virtual_camera_elapsed_ms: "
              << virtual_camera_timing.elapsed_ms << "\n";
    std::cout << "| stage | source_images | output_images | total_ms | "
                 "avg_ms_per_source_image | avg_ms_per_output_image |\n";
    std::cout << "| --- | ---: | ---: | ---: | ---: | ---: |\n";
    PrintStage("undistort_read", undistort_timing.read);
    PrintStage("undistort_process", undistort_timing.process);
    PrintStage("undistort_save", undistort_timing.save);
    PrintStage("virtual_camera_read", virtual_camera_timing.read);
    PrintStage("virtual_camera_process", virtual_camera_timing.process);
    PrintStage("virtual_camera_save", virtual_camera_timing.save);
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << "\n";
    if (argc > 0 && argv != nullptr && argv[0] != nullptr) {
      std::cerr << BuildUsage(argv[0]) << "\n";
    }
    return 1;
  }
}
