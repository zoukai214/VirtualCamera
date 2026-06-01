#include "virtual_camera/runtime_args.h"

#include <stdexcept>
#include <string>

namespace {

void Expect(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void TestParseRt024RuntimeArgsReadsRequiredFields() {
  const char* argv[] = {"rt024_tool",
                        "--dataset_root",
                        "/tmp/dataset",
                        "--config_path",
                        "/tmp/config.json",
                        "--output_root",
                        "/tmp/output"};
  const vc::Rt024RuntimeArgs args = vc::ParseRt024RuntimeArgs(7, argv);
  Expect(args.dataset_root == "/tmp/dataset", "dataset_root");
  Expect(args.config_path == "/tmp/config.json", "config_path");
  Expect(args.output_root == "/tmp/output", "output_root");
  Expect(!args.debug, "debug should default false");
  Expect(args.golden_root.empty(), "golden_root should default empty");
}

void TestParseRt024RuntimeArgsDefaultsOutputRoot() {
  const char* argv[] = {"rt024_tool", "--dataset_root", "/tmp/dataset",
                        "--config_path", "/tmp/config.json"};
  const vc::Rt024RuntimeArgs args = vc::ParseRt024RuntimeArgs(5, argv);
  Expect(args.output_root == "/tmp/dataset",
         "output_root should default to dataset_root");
}

void TestParseRt024RuntimeArgsRequiresGoldenRootWhenDebugOn() {
  const char* argv[] = {"rt024_tool",    "--dataset_root", "/tmp/dataset",
                        "--config_path", "/tmp/config.json", "--debug"};
  bool thrown = false;
  try {
    static_cast<void>(vc::ParseRt024RuntimeArgs(6, argv));
  } catch (const vc::UsageError& error) {
    const std::string message = error.what();
    thrown = message.find("golden_root") != std::string::npos &&
             message.find("--debug") != std::string::npos;
  }
  Expect(thrown, "debug should require golden_root");
}

void TestParseRt024RuntimeArgsRejectsGoldenRootWithoutDebug() {
  const char* argv[] = {"rt024_tool",    "--dataset_root", "/tmp/dataset",
                        "--config_path", "/tmp/config.json",
                        "--golden_root", "/tmp/golden"};
  bool thrown = false;
  try {
    static_cast<void>(vc::ParseRt024RuntimeArgs(7, argv));
  } catch (const vc::UsageError& error) {
    const std::string message = error.what();
    thrown = message.find("golden_root") != std::string::npos &&
             message.find("--debug") != std::string::npos;
  }
  Expect(thrown, "golden_root without debug should fail");
}

void TestParseRt024RuntimeArgsRejectsUnknownArgs() {
  const char* argv[] = {"rt024_tool",    "--dataset_root", "/tmp/dataset",
                        "--config_path", "/tmp/config.json",
                        "--unexpected",  "value"};
  bool thrown = false;
  try {
    static_cast<void>(vc::ParseRt024RuntimeArgs(7, argv));
  } catch (const vc::UsageError& error) {
    thrown = std::string(error.what()).find("--unexpected") !=
             std::string::npos;
  }
  Expect(thrown, "unknown args should fail");
}

void TestParseRt024RuntimeArgsRejectsMissingValues() {
  const char* argv[] = {"rt024_tool", "--dataset_root", "/tmp/dataset",
                        "--config_path"};
  bool thrown = false;
  try {
    static_cast<void>(vc::ParseRt024RuntimeArgs(4, argv));
  } catch (const vc::UsageError& error) {
    thrown = std::string(error.what()).find("--config_path") !=
             std::string::npos;
  }
  Expect(thrown, "missing flag value should fail");
}

}  // namespace

int main() {
  TestParseRt024RuntimeArgsReadsRequiredFields();
  TestParseRt024RuntimeArgsDefaultsOutputRoot();
  TestParseRt024RuntimeArgsRequiresGoldenRootWhenDebugOn();
  TestParseRt024RuntimeArgsRejectsGoldenRootWithoutDebug();
  TestParseRt024RuntimeArgsRejectsUnknownArgs();
  TestParseRt024RuntimeArgsRejectsMissingValues();
  return 0;
}
