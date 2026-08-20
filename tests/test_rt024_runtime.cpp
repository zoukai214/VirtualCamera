#include "virtual_camera/pipeline_config.h"
#include "virtual_camera/runtime_args.h"
#include "virtual_camera/verifier.h"

#include <functional>
#include <stdexcept>
#include <string>

namespace {

void Expect(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

vc::PipelineConfig MakeConfig() {
  vc::PipelineConfig config;
  config.paths.conf_dir_path = "calib_extract";
  return config;
}

void TestApplyRuntimeArgsKeepsConfigRuntimeIndependent() {
  vc::PipelineConfig config = MakeConfig();
  vc::RuntimeArgs args;
  args.dataset_root = "/tmp/dataset";
  args.output_root = "/tmp/output";

  vc::ApplyRuntimeArgs(args, &config);

  Expect(config.paths.conf_dir_path == "calib_extract",
         "runtime args should not mutate pipeline config");
}

void TestApplyRuntimeArgsRejectsNullConfig() {
  vc::RuntimeArgs args;

  bool thrown = false;
  try {
    vc::ApplyRuntimeArgs(args, nullptr);
  } catch (const std::invalid_argument&) {
    thrown = true;
  }
  Expect(thrown, "null config should be rejected");
}

void TestMaybeVerifyOutputsSkipsWhenDebugDisabled() {
  vc::RuntimeArgs args;
  args.debug = false;
  args.golden_root = "/tmp/golden";
  args.output_root = "/tmp/output";

  vc::PipelineConfig config;

  bool verifier_called = false;
  const vc::VerifyResult result = vc::MaybeVerifyOutputs(
      args, config,
      [&verifier_called](const std::string&, const std::string&) {
        verifier_called = true;
        return vc::VerifyResult{true, "should not run"};
      });

  Expect(result.ok, "non-debug verification should succeed");
  Expect(!verifier_called, "verifier should not run when debug is false");
}

void TestMaybeVerifyOutputsCallsVerifierWhenDebugEnabled() {
  vc::RuntimeArgs args;
  args.debug = true;
  args.golden_root = "/tmp/golden";
  args.output_root = "/tmp/output";

  vc::PipelineConfig config;

  bool verifier_called = false;
  const vc::VerifyResult result = vc::MaybeVerifyOutputs(
      args, config,
      [&verifier_called](const std::string& golden_root,
                         const std::string& actual_root) {
        verifier_called = true;
        return vc::VerifyResult{golden_root == "/tmp/golden" &&
                                    actual_root == "/tmp/output",
                                "debug verification"};
      });

  Expect(verifier_called, "verifier should run when debug is true");
  Expect(result.ok, "debug verification should return verifier result");
  Expect(result.message == "debug verification",
         "verification message should propagate");
}

}  // namespace

int main() {
  TestApplyRuntimeArgsKeepsConfigRuntimeIndependent();
  TestApplyRuntimeArgsRejectsNullConfig();
  TestMaybeVerifyOutputsSkipsWhenDebugDisabled();
  TestMaybeVerifyOutputsCallsVerifierWhenDebugEnabled();
  return 0;
}
