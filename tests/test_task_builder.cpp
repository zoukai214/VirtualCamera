#include "virtual_camera/json_utils.h"
#include "virtual_camera/task_builder.h"

#include <iostream>
#include <set>

int main() {
  const auto config = vc::ReadJson("configs/config_thor.json");
  const auto tasks = vc::BuildThorTasks(config);

  int virtual_count = 0;
  int undistort_count = 0;
  int resize_count = 0;
  std::set<std::string> bin_names;

  for (const auto& task : tasks) {
    bin_names.insert(task.bin_name);
    if (task.type == vc::TaskType::kVirtual) {
      ++virtual_count;
    } else if (task.type == vc::TaskType::kUndistort) {
      ++undistort_count;
    } else if (task.type == vc::TaskType::kResize) {
      ++resize_count;
    }
  }

  if (virtual_count != 9) {
    std::cerr << "expected 9 virtual tasks, got " << virtual_count << "\n";
    return 1;
  }
  if (undistort_count != 7) {
    std::cerr << "expected 7 undistort tasks, got " << undistort_count << "\n";
    return 1;
  }
  if (resize_count != 3) {
    std::cerr << "expected 3 resize tasks, got " << resize_count << "\n";
    return 1;
  }
  if (!bin_names.count("gdc_ft30.bin") || !bin_names.count("gdc_fw120_1080.bin")) {
    std::cerr << "expected Thor bin names are missing\n";
    return 1;
  }
  return 0;
}
