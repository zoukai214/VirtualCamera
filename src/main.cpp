#include "virtual_camera/pipeline_config.h"
#include "virtual_camera/logging.h"
#include "virtual_camera/pipeline_orchestrator.h"
#include "virtual_camera/pipeline_runner.h"
#include "virtual_camera/runtime_args.h"
#include "virtual_camera/verifier.h"

#include <opencv2/imgcodecs.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

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

cv::Mat ReadFrame(const std::filesystem::path& path) {
  const cv::Mat image = cv::imread(path.string(), cv::IMREAD_COLOR);
  if (image.empty()) {
    throw std::runtime_error("failed to read image: " + path.string());
  }
  return image;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const vc::RuntimeArgs args = vc::ParseRuntimeArgs(argc, argv);
    vc::PipelineConfig config = vc::LoadPipelineConfig(args.config_path);
    vc::LogInfo(config.showinfo != 0,
                vc::BuildPipelineStartMessage(args, config));

    vc::RunTopLevelPipelines(
        config,
        [&config, &args]() {
          const auto start_time = std::chrono::steady_clock::now();
          vc::LogInfo(config.showinfo != 0,
                      vc::BuildUndistortPipelineStartMessage(config));

          const vc::PipelineOrchestrator orchestrator(
              args.config_path, args.dataset_root,
              vc::PipelineSelection::kUndistort);
          for (const auto& task : config.undistort_tasks) {
            vc::LogInfo(config.showinfo != 0,
                        vc::BuildUndistortTaskStartMessage(task));
          }

          orchestrator.SaveUndistortArtifacts(args.output_root);

          for (const auto& source_camera :
               orchestrator.UndistortSourceInputs()) {
            const std::filesystem::path input_dir =
                std::filesystem::path(args.dataset_root) /
                config.paths.image_dir_path / source_camera.image_dir;
            for (const auto& image_path : ListFiles(input_dir)) {
              const cv::Mat image = ReadFrame(image_path);
              const std::vector<vc::UndistortFrameResult> results =
                  orchestrator.ProcessUndistortFrame(source_camera.camera_id,
                                                     image);
              orchestrator.SaveUndistortFrameResults(
                  results, args.output_root,
                  image_path.filename().string());
            }
          }

          for (const auto& task : config.undistort_tasks) {
            vc::LogInfo(config.showinfo != 0,
                        vc::BuildUndistortTaskDoneMessage(
                            task, vc::ElapsedMilliseconds(start_time)));
          }
          vc::LogInfo(config.showinfo != 0,
                      vc::BuildUndistortPipelineDoneMessage(
                          vc::ElapsedMilliseconds(start_time)));
        },
        [&config, &args]() {
          const auto start_time = std::chrono::steady_clock::now();
          vc::LogInfo(config.showinfo != 0,
                      vc::BuildVirtualCameraPipelineStartMessage(config));

          const vc::PipelineOrchestrator orchestrator(
              args.config_path, args.dataset_root,
              vc::PipelineSelection::kVirtualCamera);
          for (const auto& task : config.virtual_tasks) {
            vc::LogInfo(config.showinfo != 0,
                        vc::BuildVirtualTaskStartMessage(task));
          }

          orchestrator.SaveVirtualCameraArtifacts(args.output_root);

          for (const auto& source_camera : orchestrator.VirtualSourceInputs()) {
            const std::filesystem::path input_dir =
                std::filesystem::path(args.dataset_root) /
                config.paths.image_dir_path / source_camera.image_dir;
            for (const auto& image_path : ListFiles(input_dir)) {
              const cv::Mat image = ReadFrame(image_path);
              const std::vector<vc::VirtualCameraFrameResult> results =
                  orchestrator.ProcessVirtualCameraFrame(
                      source_camera.camera_id, image);
              orchestrator.SaveVirtualCameraFrameResults(
                  results, args.output_root,
                  image_path.filename().string());
            }
          }

          for (const auto& task : config.virtual_tasks) {
            vc::LogInfo(config.showinfo != 0,
                        vc::BuildVirtualTaskDoneMessage(
                            task, vc::ElapsedMilliseconds(start_time)));
          }
          vc::LogInfo(config.showinfo != 0,
                      vc::BuildVirtualCameraPipelineDoneMessage(
                          vc::ElapsedMilliseconds(start_time)));
        });

    const vc::VerifyResult result = vc::MaybeVerifyOutputs(
        args, config,
        [](const std::string& golden_root, const std::string& actual_root) {
          return vc::VerifyOutputs(golden_root, actual_root);
        });
    if (!result.ok) {
      std::cerr << result.message << "\n";
      return 2;
    }
    std::cout << result.message << "\n";
    return 0;
  } catch (const vc::UsageError& error) {
    std::cerr << error.what() << "\n";
    if (std::string(error.what()).find("Usage: ") == std::string::npos) {
      const std::string program_name =
          (argc > 0 && argv != nullptr && argv[0] != nullptr) ? argv[0]
                                                              : "virtual_camera_tool";
      std::cerr << vc::BuildUsage(program_name) << "\n";
    }
    return 1;
  } catch (const std::exception& error) {
    std::cerr << error.what() << "\n";
    return 1;
  }
}
