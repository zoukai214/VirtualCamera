#include "virtual_camera/pipeline_config.h"
#include "virtual_camera/pipeline_runner.h"
#include "virtual_camera/runtime_args.h"
#include "virtual_camera/undistort_processor.h"
#include "virtual_camera/verifier.h"
#include "virtual_camera/virtual_camera_processor.h"

#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
  try {
    const vc::Rt024RuntimeArgs args = vc::ParseRt024RuntimeArgs(argc, argv);
    vc::PipelineConfig config = vc::LoadPipelineConfig(args.config_path);
    vc::ApplyRt024RuntimeArgs(args, &config);

    vc::RunTopLevelPipelines(
        config,
        [&config]() { vc::RunUndistortPipeline(config); },
        [&config]() { vc::RunVirtualCameraPipeline(config); });

    const vc::VerifyResult result = vc::MaybeVerifyRt024Outputs(
        args, config,
        [](const std::string& golden_root, const std::string& actual_root) {
          return vc::VerifyRt024Outputs(golden_root, actual_root);
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
      std::cerr << vc::BuildRt024Usage(program_name) << "\n";
    }
    return 1;
  } catch (const std::exception& error) {
    std::cerr << error.what() << "\n";
    return 1;
  }
}
