#include "virtual_camera/runtime_args.h"

#include <functional>
#include <stdexcept>
#include <string>

namespace vc {
namespace {

UsageError MakeUsageError(const std::string& message,
                          const std::string& program_name) {
  return UsageError(message + "\n" + BuildRt024Usage(program_name));
}

const char* RequireValue(int argc, const char* const* argv, int index,
                         const std::string& flag) {
  if (index + 1 >= argc) {
    throw MakeUsageError("missing value for " + flag, argv[0]);
  }
  const char* value = argv[index + 1];
  if (value != nullptr && std::string(value).rfind("--", 0) == 0) {
    throw MakeUsageError("missing value for " + flag, argv[0]);
  }
  return value;
}

}  // namespace

std::string BuildRt024Usage(const std::string& program_name) {
  return "Usage: " + program_name +
         " --dataset_root <path> --config_path <path> "
         "[--output_root <path>] [--debug --golden_root <path>]";
}

Rt024RuntimeArgs ParseRt024RuntimeArgs(int argc, const char* const* argv) {
  if (argc <= 0 || argv == nullptr || argv[0] == nullptr) {
    throw UsageError("invalid argv");
  }

  Rt024RuntimeArgs args;
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
    if (flag == "--golden_root") {
      args.golden_root = RequireValue(argc, argv, index, flag);
      ++index;
      continue;
    }
    if (flag == "--debug") {
      args.debug = true;
      continue;
    }
    throw MakeUsageError("unknown argument: " + flag, argv[0]);
  }

  if (args.dataset_root.empty()) {
    throw MakeUsageError("missing required flag --dataset_root", argv[0]);
  }
  if (args.config_path.empty()) {
    throw MakeUsageError("missing required flag --config_path", argv[0]);
  }
  if (args.output_root.empty()) {
    args.output_root = args.dataset_root;
  }
  if (args.debug && args.golden_root.empty()) {
    throw MakeUsageError("--debug requires --golden_root", argv[0]);
  }
  if (!args.debug && !args.golden_root.empty()) {
    throw MakeUsageError("--golden_root requires --debug", argv[0]);
  }

  return args;
}

void ApplyRt024RuntimeArgs(const Rt024RuntimeArgs& args, PipelineConfig* config) {
  if (config == nullptr) {
    throw std::invalid_argument("config must not be null");
  }

  config->dataset_root = args.dataset_root;
  config->output_root = args.output_root.empty() ? args.dataset_root : args.output_root;
  config->paths.dataset_root = config->dataset_root;
}

VerifyResult MaybeVerifyRt024Outputs(
    const Rt024RuntimeArgs& args, const PipelineConfig& config,
    const std::function<VerifyResult(const std::string&, const std::string&)>&
        verifier) {
  if (!args.debug) {
    return VerifyResult{true, "verification skipped (debug disabled)"};
  }
  return verifier(args.golden_root, config.output_root);
}

}  // namespace vc
