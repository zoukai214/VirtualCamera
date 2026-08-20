#include "virtual_camera/verifier.h"

#include "virtual_camera/json_utils.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace vc {
namespace {

std::vector<std::filesystem::path> ListRelativeFiles(const std::filesystem::path& root,
                                                     const std::string& extension) {
  std::vector<std::filesystem::path> files;
  if (!std::filesystem::exists(root)) {
    return files;
  }
  for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
    if (entry.is_regular_file() && entry.path().extension() == extension) {
      files.push_back(std::filesystem::relative(entry.path(), root));
    }
  }
  std::sort(files.begin(), files.end());
  return files;
}

bool SameBytes(const std::filesystem::path& left, const std::filesystem::path& right,
               std::uintmax_t* offset) {
  const auto left_size = std::filesystem::file_size(left);
  const auto right_size = std::filesystem::file_size(right);
  if (left_size != right_size) {
    *offset = std::min(left_size, right_size);
    return false;
  }

  std::ifstream a(left, std::ios::binary);
  std::ifstream b(right, std::ios::binary);
  char ca = 0;
  char cb = 0;
  std::uintmax_t index = 0;
  while (a.get(ca) && b.get(cb)) {
    if (ca != cb) {
      *offset = index;
      return false;
    }
    ++index;
  }
  return true;
}

void CompareJsonValue(const nlohmann::json& golden, const nlohmann::json& actual,
                      const std::string& path, std::ostringstream* errors) {
  const nlohmann::json::json_pointer pointer(path);
  const bool golden_has = golden.contains(pointer);
  const bool actual_has = actual.contains(pointer);
  if (!golden_has && !actual_has) {
    return;
  }
  if (golden_has != actual_has) {
    *errors << "json key missing: " << path << "\n";
    return;
  }
  if (golden.at(pointer) != actual.at(pointer)) {
    *errors << "json field mismatch: " << path << "\n";
  }
}

void CompareVirtualJson(const std::filesystem::path& golden_root,
                        const std::filesystem::path& actual_root,
                        std::ostringstream* errors) {
  const auto golden_files = ListRelativeFiles(golden_root / "calib/virtual", ".json");
  const auto actual_files = ListRelativeFiles(actual_root / "calib/virtual", ".json");
  if (golden_files != actual_files) {
    *errors << "file set mismatch: calib/virtual\n";
    return;
  }

  for (const auto& rel : golden_files) {
    const auto golden_path = golden_root / "calib/virtual" / rel;
    const auto actual_path = actual_root / "calib/virtual" / rel;
    const auto golden = ReadJson(golden_path.string());
    const auto actual = ReadJson(actual_path.string());
    CompareJsonValue(golden, actual, "/value0/param/cam_K/data", errors);
    CompareJsonValue(golden, actual, "/value0/param/cam_K_new/data", errors);
    CompareJsonValue(golden, actual, "/value0/param/cam_dist/data", errors);
    CompareJsonValue(golden, actual, "/value0/param/img_dist_w", errors);
    CompareJsonValue(golden, actual, "/value0/param/img_dist_h", errors);
    CompareJsonValue(golden, actual, "/value0/param/img_new_w", errors);
    CompareJsonValue(golden, actual, "/value0/param/img_new_h", errors);
    CompareJsonValue(golden, actual, "/value0/param/sensor_calib/data", errors);
  }
}

void CompareBinDirectory(const std::filesystem::path& golden_root,
                         const std::filesystem::path& actual_root,
                         const std::string& subdir,
                         std::ostringstream* errors) {
  const auto golden_files = ListRelativeFiles(golden_root / subdir, ".bin");
  const auto actual_files = ListRelativeFiles(actual_root / subdir, ".bin");
  if (golden_files != actual_files) {
    *errors << "file set mismatch: " << subdir << "\n";
    return;
  }
  for (const auto& rel : golden_files) {
    std::uintmax_t offset = 0;
    const auto golden_path = golden_root / subdir / rel;
    const auto actual_path = actual_root / subdir / rel;
    if (!SameBytes(golden_path, actual_path, &offset)) {
      *errors << "byte mismatch: " << subdir << "/" << rel.string()
              << " at offset " << offset << "\n";
    }
  }
}

void CompareJsonDirectory(const std::filesystem::path& golden_root,
                          const std::filesystem::path& actual_root,
                          const std::string& subdir,
                          std::ostringstream* errors) {
  const auto golden_files = ListRelativeFiles(golden_root / subdir, ".json");
  const auto actual_files = ListRelativeFiles(actual_root / subdir, ".json");
  if (golden_files != actual_files) {
    *errors << "file set mismatch: " << subdir << "\n";
    return;
  }

  for (const auto& rel : golden_files) {
    const auto golden_json = ReadJson((golden_root / subdir / rel).string());
    const auto actual_json = ReadJson((actual_root / subdir / rel).string());
    const auto approx_equal = [](const auto& self, const nlohmann::json& left,
                                 const nlohmann::json& right) -> bool {
      if (left.type() != right.type()) {
        if (left.is_number() && right.is_number()) {
          return std::fabs(left.get<double>() - right.get<double>()) <= 1e-9;
        }
        return false;
      }
      if (left.is_number()) {
        return std::fabs(left.get<double>() - right.get<double>()) <= 1e-9;
      }
      if (left.is_array()) {
        if (left.size() != right.size()) {
          return false;
        }
        for (std::size_t index = 0; index < left.size(); ++index) {
          if (!self(self, left.at(index), right.at(index))) {
            return false;
          }
        }
        return true;
      }
      if (left.is_object()) {
        if (left.size() != right.size()) {
          return false;
        }
        for (const auto& item : left.items()) {
          if (!right.contains(item.key()) ||
              !self(self, item.value(), right.at(item.key()))) {
            return false;
          }
        }
        return true;
      }
      return left == right;
    };
    if (!approx_equal(approx_equal, golden_json, actual_json)) {
      *errors << "json mismatch: " << subdir << "/" << rel.string() << "\n";
    }
  }
}

}  // namespace

VerifyResult VerifyGdcOutputs(const std::string& golden_root, const std::string& actual_root) {
  std::ostringstream errors;
  CompareBinDirectory(golden_root, actual_root, "calib/gdc", &errors);
  CompareBinDirectory(golden_root, actual_root, "calib/gdc_intri", &errors);
  CompareVirtualJson(golden_root, actual_root, &errors);

  const std::string message = errors.str();
  if (!message.empty()) {
    return {false, message};
  }
  return {true, "verification passed"};
}

VerifyResult VerifyOutputs(const std::string& golden_root,
                                const std::string& actual_root) {
  std::ostringstream errors;
  CompareBinDirectory(golden_root, actual_root, "vc_gdcbin_dir_path", &errors);
  CompareJsonDirectory(golden_root, actual_root, "calib_virtual_camera", &errors);
  CompareJsonDirectory(golden_root, actual_root, "calib_undistortion", &errors);

  const std::string message = errors.str();
  if (!message.empty()) {
    return {false, message};
  }
  return {true, "verification passed"};
}

}  // namespace vc
