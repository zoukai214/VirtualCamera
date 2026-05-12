#include "virtual_camera/jobs.h"

#include <atomic>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Expect(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void TestRunJobsSequential() {
  std::atomic<int> count{0};
  std::vector<std::function<void()>> jobs;
  for (int index = 0; index < 5; ++index) {
    jobs.push_back([&count]() { ++count; });
  }
  vc::RunJobs(jobs, 1);
  Expect(count.load() == 5, "sequential jobs");
}

void TestRunJobsParallel() {
  std::atomic<int> count{0};
  std::vector<std::function<void()>> jobs;
  for (int index = 0; index < 8; ++index) {
    jobs.push_back([&count]() { ++count; });
  }
  vc::RunJobs(jobs, 3);
  Expect(count.load() == 8, "parallel jobs");
}

void TestRunJobsPropagatesException() {
  std::vector<std::function<void()>> jobs;
  jobs.push_back([]() {});
  jobs.push_back([]() { throw std::runtime_error("boom"); });
  jobs.push_back([]() {});

  bool thrown = false;
  try {
    vc::RunJobs(jobs, 2);
  } catch (const std::runtime_error& error) {
    thrown = std::string(error.what()).find("boom") != std::string::npos;
  }
  Expect(thrown, "RunJobs should propagate exception");
}

}  // namespace

int main() {
  TestRunJobsSequential();
  TestRunJobsParallel();
  TestRunJobsPropagatesException();
  return 0;
}
