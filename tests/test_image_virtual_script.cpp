#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include <cstdlib>
#include <sys/wait.h>

namespace {

void Expect(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void WriteText(const std::filesystem::path& path, const std::string& content) {
  std::ofstream output(path);
  output << content;
}

int RunCommand(const std::string& command) {
  const int status = std::system(command.c_str());
  if (status == -1) {
    throw std::runtime_error("failed to run command");
  }
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  throw std::runtime_error("command did not exit normally");
}

std::string ReadText(const std::filesystem::path& path) {
  std::ifstream input(path);
  return std::string((std::istreambuf_iterator<char>(input)),
                     std::istreambuf_iterator<char>());
}

void TestImageVirtualScriptAvoidsLiteralBuildPath() {
  const std::string script = ReadText("image_virtual.bash");
  Expect(script.find("build/") == std::string::npos,
         "script should avoid literal build/ path so packaged script stays standalone");
}

void TestImageVirtualScriptUsesBuildBinaryWhenPresent() {
  const std::filesystem::path root = "build/test_tmp/image_virtual_script";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root / "build");

  const std::filesystem::path script_source = "image_virtual.bash";
  const std::filesystem::path script_copy = root / "image_virtual.bash";
  std::filesystem::copy_file(script_source, script_copy,
                             std::filesystem::copy_options::overwrite_existing);
  std::filesystem::permissions(
      script_copy,
      std::filesystem::perms::owner_read | std::filesystem::perms::owner_write |
          std::filesystem::perms::owner_exec |
          std::filesystem::perms::group_read |
          std::filesystem::perms::group_exec |
          std::filesystem::perms::others_read |
          std::filesystem::perms::others_exec,
      std::filesystem::perm_options::replace);

  const std::filesystem::path args_log = root / "tool_args.txt";
  const std::filesystem::path tool_path = root / "build" / "virtual_camera_tool";
  WriteText(tool_path,
            "#!/usr/bin/env bash\n"
            "printf '%s\\n' \"$@\" > \"" +
                args_log.string() + "\"\n");
  std::filesystem::permissions(
      tool_path,
      std::filesystem::perms::owner_read | std::filesystem::perms::owner_write |
          std::filesystem::perms::owner_exec |
          std::filesystem::perms::group_read |
          std::filesystem::perms::group_exec |
          std::filesystem::perms::others_read |
          std::filesystem::perms::others_exec,
      std::filesystem::perm_options::replace);

  const int exit_code = RunCommand(
      "bash " + script_copy.string() +
      " --dataset_root /tmp/dataset"
      " --config_path /tmp/config.json"
      " --output_root /tmp/output"
      " --debug"
      " --golden_root /tmp/golden");

  Expect(exit_code == 0, "script should use build/virtual_camera_tool");
  const std::string args = ReadText(args_log);
  Expect(args.find("--dataset_root\n/tmp/dataset\n") != std::string::npos,
         "tool should receive dataset_root");
  Expect(args.find("--config_path\n/tmp/config.json\n") != std::string::npos,
         "tool should receive config_path");
  Expect(args.find("--output_root\n/tmp/output\n") != std::string::npos,
         "tool should receive output_root");
  Expect(args.find("--debug\n") != std::string::npos,
         "tool should receive debug flag");
  Expect(args.find("--golden_root\n/tmp/golden\n") != std::string::npos,
         "tool should receive golden_root");
}

}  // namespace

int main() {
  TestImageVirtualScriptAvoidsLiteralBuildPath();
  TestImageVirtualScriptUsesBuildBinaryWhenPresent();
  return 0;
}
