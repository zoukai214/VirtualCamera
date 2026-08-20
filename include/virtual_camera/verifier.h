#pragma once

#include <string>

namespace vc {

struct VerifyResult {
  bool ok = false;
  std::string message;
};

VerifyResult VerifyGdcOutputs(const std::string& golden_root, const std::string& actual_root);
VerifyResult VerifyOutputs(const std::string& golden_root, const std::string& actual_root);

}  // namespace vc
