#pragma once

#include <string>

namespace vc {

struct VerifyResult {
  bool ok = false;
  std::string message;
};

VerifyResult VerifyOutputs(const std::string& golden_root, const std::string& actual_root);
VerifyResult VerifyRt024Outputs(const std::string& golden_root, const std::string& actual_root);

}  // namespace vc
