#pragma once

#include <stdexcept>
#include <string>

namespace vc {

class UsageError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

struct Rt024RuntimeArgs {
  std::string dataset_root;
  std::string config_path;
  std::string output_root;
  bool debug = false;
  std::string golden_root;
};

std::string BuildRt024Usage(const std::string& program_name);
Rt024RuntimeArgs ParseRt024RuntimeArgs(int argc, const char* const* argv);

}  // namespace vc
