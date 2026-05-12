#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace vc {

struct JobsConfig {
  int jobs = 0;
  bool auto_jobs = true;
};

inline bool ParseNonNegativeInt(const char* text, int* value) {
  if (text == nullptr || *text == '\0') {
    return false;
  }
  int parsed = 0;
  for (const char* cursor = text; *cursor != '\0'; ++cursor) {
    if (*cursor < '0' || *cursor > '9') {
      return false;
    }
    parsed = parsed * 10 + (*cursor - '0');
  }
  *value = parsed;
  return true;
}

inline bool ParseOptionalJobs(int argc, const char* const* argv,
                              int first_optional, JobsConfig* config) {
  if (config == nullptr) {
    return false;
  }

  config->jobs = 0;
  config->auto_jobs = true;
  if (argc == first_optional) {
    return true;
  }
  if (argc != first_optional + 2 ||
      std::string(argv[first_optional]) != "--jobs") {
    return false;
  }

  int parsed = 0;
  if (!ParseNonNegativeInt(argv[first_optional + 1], &parsed)) {
    return false;
  }
  config->jobs = parsed;
  config->auto_jobs = parsed == 0;
  return true;
}

inline int ResolveJobs(const JobsConfig& config, unsigned int hardware_jobs) {
  if (!config.auto_jobs) {
    return std::max(1, config.jobs);
  }
  return hardware_jobs > 0 ? static_cast<int>(hardware_jobs) : 1;
}

inline void RunJobs(const std::vector<std::function<void()>>& jobs,
                    int max_jobs) {
  if (jobs.empty()) {
    return;
  }

  if (max_jobs <= 1) {
    for (const auto& job : jobs) {
      job();
    }
    return;
  }

  const std::size_t worker_count =
      std::min<std::size_t>(jobs.size(),
                            static_cast<std::size_t>(std::max(1, max_jobs)));
  std::atomic<std::size_t> next{0};
  std::mutex error_mutex;
  std::vector<std::string> errors;
  std::vector<std::thread> workers;
  workers.reserve(worker_count);

  for (std::size_t worker_index = 0; worker_index < worker_count;
       ++worker_index) {
    workers.emplace_back([&]() {
      while (true) {
        const std::size_t index = next.fetch_add(1);
        if (index >= jobs.size()) {
          break;
        }
        try {
          jobs[index]();
        } catch (const std::exception& ex) {
          std::lock_guard<std::mutex> lock(error_mutex);
          errors.push_back(ex.what());
        } catch (...) {
          std::lock_guard<std::mutex> lock(error_mutex);
          errors.push_back("unknown exception");
        }
      }
    });
  }

  for (auto& worker : workers) {
    worker.join();
  }

  if (!errors.empty()) {
    throw std::runtime_error("parallel task failed: " + errors.front());
  }
}

template <typename Fn>
void ParallelFor(std::size_t count, int jobs, Fn fn) {
  std::vector<std::function<void()>> work_items;
  work_items.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    work_items.push_back([index, &fn]() { fn(index); });
  }
  RunJobs(work_items, jobs);
}

}  // namespace vc
