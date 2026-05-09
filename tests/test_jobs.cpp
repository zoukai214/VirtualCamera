#include "virtual_camera/jobs.h"

#include <atomic>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

bool ExpectEqual(int actual, int expected, const std::string& context) {
  if (actual != expected) {
    std::cerr << context << ": expected " << expected << ", got " << actual
              << "\n";
    return false;
  }
  return true;
}

bool TestParseJobs() {
  vc::JobsConfig config;
  const char* default_argv[] = {"tool", "generate-4v", "config.yaml"};
  if (!vc::ParseOptionalJobs(3, default_argv, 3, &config)) {
    std::cerr << "default jobs should parse\n";
    return false;
  }
  if (!config.auto_jobs || config.jobs != 0) {
    std::cerr << "default jobs should use auto mode\n";
    return false;
  }

  const char* explicit_argv[] = {"tool", "generate-4v", "config.yaml",
                                 "--jobs", "4"};
  if (!vc::ParseOptionalJobs(5, explicit_argv, 3, &config)) {
    std::cerr << "--jobs 4 should parse\n";
    return false;
  }
  if (config.auto_jobs || config.jobs != 4) {
    std::cerr << "--jobs 4 should produce explicit jobs=4\n";
    return false;
  }

  const char* auto_argv[] = {"tool", "generate-4v", "config.yaml", "--jobs",
                             "0"};
  if (!vc::ParseOptionalJobs(5, auto_argv, 3, &config)) {
    std::cerr << "--jobs 0 should parse\n";
    return false;
  }
  if (!config.auto_jobs || config.jobs != 0) {
    std::cerr << "--jobs 0 should use auto mode\n";
    return false;
  }

  const char* bad_argv[] = {"tool", "generate-4v", "config.yaml", "--jobs",
                            "-1"};
  if (vc::ParseOptionalJobs(5, bad_argv, 3, &config)) {
    std::cerr << "--jobs -1 should fail\n";
    return false;
  }

  const char* missing_argv[] = {"tool", "generate-4v", "config.yaml",
                                "--jobs"};
  if (vc::ParseOptionalJobs(4, missing_argv, 3, &config)) {
    std::cerr << "missing jobs value should fail\n";
    return false;
  }

  return true;
}

bool TestResolveJobs() {
  if (!ExpectEqual(vc::ResolveJobs({0, true}, 8), 8, "auto jobs")) {
    return false;
  }
  if (!ExpectEqual(vc::ResolveJobs({0, true}, 0), 1, "auto fallback")) {
    return false;
  }
  if (!ExpectEqual(vc::ResolveJobs({3, false}, 8), 3, "explicit jobs")) {
    return false;
  }
  return true;
}

bool TestParallelForRunsAllItems() {
  std::atomic<int> count{0};
  vc::ParallelFor(16, 4, [&](std::size_t) { ++count; });
  return ExpectEqual(count.load(), 16, "parallel count");
}

bool TestParallelForPropagatesException() {
  try {
    vc::ParallelFor(4, 2, [](std::size_t index) {
      if (index == 2) {
        throw std::runtime_error("boom");
      }
    });
  } catch (const std::runtime_error& ex) {
    const std::string message = ex.what();
    if (message.find("parallel task failed") != std::string::npos &&
        message.find("boom") != std::string::npos) {
      return true;
    }
    std::cerr << "unexpected exception message: " << message << "\n";
    return false;
  }
  std::cerr << "expected exception\n";
  return false;
}

}  // namespace

int main() {
  if (!TestParseJobs()) return 1;
  if (!TestResolveJobs()) return 1;
  if (!TestParallelForRunsAllItems()) return 1;
  if (!TestParallelForPropagatesException()) return 1;
  return 0;
}
