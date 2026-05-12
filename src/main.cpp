#include "virtual_camera/jobs.h"
#include "virtual_camera/pipeline_config.h"
#include "virtual_camera/undistort_processor.h"
#include "virtual_camera/verifier.h"
#include "virtual_camera/virtual_camera_processor.h"

#include <functional>
#include <iostream>
#include <stdexcept>
#include <vector>

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <config.json>\n";
    return 1;
  }

  try {
    const vc::PipelineConfig config = vc::LoadPipelineConfig(argv[1]);
    std::vector<std::function<void()>> pipeline_jobs;
    if (config.process_undistort != 0) {
      pipeline_jobs.push_back([&config]() { vc::RunUndistortPipeline(config); });
    }
    if (config.process_virtual_camera != 0) {
      pipeline_jobs.push_back(
          [&config]() { vc::RunVirtualCameraPipeline(config); });
    }

    vc::RunJobs(pipeline_jobs, config.task_parallelism);

    const vc::VerifyResult result =
        vc::VerifyRt024Outputs(config.golden_root, config.output_root);
    if (!result.ok) {
      std::cerr << result.message << "\n";
      return 2;
    }
    std::cout << result.message << "\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << "\n";
    return 1;
  }
}
