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
  config.dataset_root = "/stale/dataset";
  config.output_root = "/stale/output";
  config.paths.dataset_root = "/stale/path";
  return config;
}

void TestApplyRuntimeArgsSetsRuntimeRoots() {
  vc::PipelineConfig config = MakeConfig();
  vc::RuntimeArgs args;
  args.dataset_root = "/tmp/dataset";
  args.output_root = "/tmp/output";

  vc::ApplyRuntimeArgs(args, &config);

  Expect(config.dataset_root == "/tmp/dataset", "dataset_root should update");
  Expect(config.output_root == "/tmp/output", "output_root should update");
  Expect(config.paths.dataset_root == "/tmp/dataset",
         "paths.dataset_root should update");
}

void TestApplyRuntimeArgsDefaultsOutputRootToDatasetRoot() {
  vc::PipelineConfig config = MakeConfig();
  vc::RuntimeArgs args;
  args.dataset_root = "/tmp/dataset";

  vc::ApplyRuntimeArgs(args, &config);

  Expect(config.dataset_root == "/tmp/dataset", "dataset_root should update");
  Expect(config.output_root == "/tmp/dataset",
         "output_root should default to dataset_root");
  Expect(config.paths.dataset_root == "/tmp/dataset",
         "paths.dataset_root should default to dataset_root");
}

void TestMaybeVerifyOutputsSkipsWhenDebugDisabled() {
  vc::RuntimeArgs args;
  args.debug = false;
  args.golden_root = "/tmp/golden";

  vc::PipelineConfig config;
  config.output_root = "/tmp/output";

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

  vc::PipelineConfig config;
  config.output_root = "/tmp/output";

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
  TestApplyRuntimeArgsSetsRuntimeRoots();
  TestApplyRuntimeArgsDefaultsOutputRootToDatasetRoot();
  TestMaybeVerifyOutputsSkipsWhenDebugDisabled();
  TestMaybeVerifyOutputsCallsVerifierWhenDebugEnabled();
  return 0;
}
