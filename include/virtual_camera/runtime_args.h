#pragma once

#include "virtual_camera/pipeline_config.h"
#include "virtual_camera/verifier.h"

#include <functional>
#include <stdexcept>
#include <string>

namespace vc {

class UsageError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

struct RuntimeArgs {
  std::string dataset_root;
  std::string config_path;
  std::string output_root;
  bool debug = false;
  std::string golden_root;
};

std::string BuildUsage(const std::string& program_name);
// debug 模式下要求 output_root 与 golden_root 不能相同。
RuntimeArgs ParseRuntimeArgs(int argc, const char* const* argv);
void ApplyRuntimeArgs(const RuntimeArgs& args, PipelineConfig* config);
VerifyResult MaybeVerifyOutputs(
    const RuntimeArgs& args, const PipelineConfig& config,
    const std::function<VerifyResult(const std::string&, const std::string&)>&
        verifier);

}  // namespace vc
