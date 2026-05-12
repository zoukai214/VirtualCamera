#include "virtual_camera/jobs.h"

#include <array>
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

void TestRunJobsSequentialPropagatesAfterAllJobs() {
  std::atomic<int> count{0};
  std::vector<std::function<void()>> jobs;
  jobs.push_back([&count]() { ++count; });
  jobs.push_back([&count]() { ++count; throw std::runtime_error("boom"); });
  jobs.push_back([&count]() { ++count; });

  bool thrown = false;
  try {
    vc::RunJobs(jobs, 1);
  } catch (const std::runtime_error& error) {
    thrown = std::string(error.what()).find("parallel task failed") !=
                 std::string::npos &&
             std::string(error.what()).find("boom") != std::string::npos;
  }
  Expect(thrown, "RunJobs sequential exception should be wrapped");
  Expect(count.load() == 3, "RunJobs sequential should finish all jobs");
}

void TestRunJobsEmptyNoOp() {
  std::vector<std::function<void()>> jobs;
  vc::RunJobs(jobs, 4);
}

void TestRunJobsZeroBehavesLikeSerial() {
  std::atomic<int> count{0};
  std::vector<std::function<void()>> jobs;
  jobs.push_back([&count]() { ++count; });
  jobs.push_back([&count]() { ++count; throw std::runtime_error("boom"); });
  jobs.push_back([&count]() { ++count; });

  bool thrown = false;
  try {
    vc::RunJobs(jobs, 0);
  } catch (const std::runtime_error& error) {
    const std::string message = error.what();
    thrown = message.find("parallel task failed") != std::string::npos &&
             message.find("boom") != std::string::npos;
  }
  Expect(thrown, "RunJobs zero should wrap exception");
  Expect(count.load() == 3, "RunJobs zero should finish all jobs");
}

void TestRunJobsOversubscribedStillRunsEachJobOnce() {
  std::atomic<int> count{0};
  std::vector<std::function<void()>> jobs;
  for (int index = 0; index < 3; ++index) {
    jobs.push_back([&count]() { ++count; });
  }
  vc::RunJobs(jobs, 8);
  Expect(count.load() == 3, "RunJobs oversubscribed should run each job once");
}

void TestRunJobsHandlesManyImageTasks() {
  std::array<std::atomic<int>, 16> hits{};
  std::vector<std::function<void()>> jobs;
  for (int index = 0; index < 16; ++index) {
    jobs.push_back([index, &hits]() { hits[index].fetch_add(1, std::memory_order_relaxed); });
  }
  vc::RunJobs(jobs, 4);
  for (int index = 0; index < 16; ++index) {
    Expect(hits[index].load(std::memory_order_relaxed) == 1,
           "image jobs unique execution");
  }
}

}  // namespace

int main() {
  TestRunJobsSequential();
  TestRunJobsParallel();
  TestRunJobsPropagatesException();
  TestRunJobsSequentialPropagatesAfterAllJobs();
  TestRunJobsEmptyNoOp();
  TestRunJobsZeroBehavesLikeSerial();
  TestRunJobsOversubscribedStillRunsEachJobOnce();
  TestRunJobsHandlesManyImageTasks();
  return 0;
}
