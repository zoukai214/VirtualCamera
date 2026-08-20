#include "virtual_camera/pipeline_config.h"
#include "virtual_camera/logging.h"
#include "virtual_camera/pipeline_runner.h"
#include "virtual_camera/runtime_args.h"
#include "virtual_camera/undistort_processor.h"
#include "virtual_camera/verifier.h"
#include "virtual_camera/virtual_camera_processor.h"

#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
  try {
    const vc::RuntimeArgs args = vc::ParseRuntimeArgs(argc, argv);
    vc::PipelineConfig config = vc::LoadPipelineConfig(args.config_path);
    vc::ApplyRuntimeArgs(args, &config);
    vc::LogInfo(config.showinfo != 0,
                vc::BuildPipelineStartMessage(args, config));

    vc::RunTopLevelPipelines(
        config,
        [&config]() { vc::RunUndistortPipeline(config); },
        [&config]() { vc::RunVirtualCameraPipeline(config); });

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
