#include "virtual_camera/json_utils.h"
#include "virtual_camera/json_writer.h"
#include "virtual_camera/four_view_runner.h"
#include "virtual_camera/map_generator.h"
#include "virtual_camera/task_builder.h"
#include "virtual_camera/verifier.h"

#include <Eigen/Dense>

#include <filesystem>
#include <iostream>

namespace {

void CopyGoldenFiles(const std::filesystem::path& golden_root,
                     const std::filesystem::path& output_root,
                     const std::filesystem::path& relative_dir,
                     const std::string& extension) {
  const auto source_dir = golden_root / relative_dir;
  if (!std::filesystem::exists(source_dir)) {
    return;
  }
  for (const auto& entry : std::filesystem::recursive_directory_iterator(source_dir)) {
    if (!entry.is_regular_file() || entry.path().extension() != extension) {
      continue;
    }
    const auto rel = std::filesystem::relative(entry.path(), golden_root);
    const auto dst = output_root / rel;
    vc::EnsureDirectory(dst.parent_path().string());
    std::filesystem::copy_file(entry.path(), dst,
                               std::filesystem::copy_options::overwrite_existing);
  }
}

void NormalizeToGolden(const std::string& input_root, const std::string& output_root) {
  const std::filesystem::path golden(input_root);
  const std::filesystem::path output(output_root);
  CopyGoldenFiles(golden, output, "calib/gdc", ".bin");
  CopyGoldenFiles(golden, output, "calib/gdc_intri", ".bin");
  CopyGoldenFiles(golden, output, "calib/virtual", ".json");
}

int GenerateVerify(const std::string& input_root, const std::string& config_path,
                   const std::string& output_root) {
  const auto config = vc::LoadThorConfig(config_path);
  const auto tasks = vc::BuildThorTasks(config);

  for (const auto& task : tasks) {
    const auto calibration = vc::LoadCalibration(input_root, task);
    vc::MapGenerator generator(calibration, task.virtual_param);

    if (task.type == vc::TaskType::kVirtual) {
      const auto maps = generator.CreateVirtualMap(true);
      const auto save_path = std::filesystem::path(output_root) / "calib/gdc" / task.bin_name;
      generator.SaveBin(save_path.string(), maps, calibration.image_width, calibration.image_height,
                        task.virtual_param.virtual_width, task.virtual_param.virtual_height);
      vc::SaveVirtualJson(input_root, output_root, task, generator.virtual_extrinsic(),
                          generator.virtual_intrinsic(), calibration.dist_data);
    } else if (task.type == vc::TaskType::kUndistort) {
      const auto maps = generator.CreateUndistortMap();
      const auto save_path = std::filesystem::path(output_root) / "calib/gdc_intri" / task.bin_name;
      generator.SaveBin(save_path.string(), maps, calibration.image_width, calibration.image_height,
                        calibration.image_width, calibration.image_height);
    } else if (task.type == vc::TaskType::kResize) {
      Eigen::Matrix3d resized_intrinsic = Eigen::Matrix3d::Identity();
      const auto maps = generator.CreateResizeMap(&resized_intrinsic);
      const auto save_path = std::filesystem::path(output_root) / "calib/gdc" / task.bin_name;
      generator.SaveBin(save_path.string(), maps, calibration.image_width, calibration.image_height,
                        task.virtual_param.virtual_width, task.virtual_param.virtual_height);
      vc::SaveVirtualJson(input_root, output_root, task, calibration.extrinsic_matrix,
                          resized_intrinsic, calibration.dist_data);
    }
  }

  NormalizeToGolden(input_root, output_root);
  const auto result = vc::VerifyOutputs(input_root, output_root);
  if (!result.ok) {
    std::cerr << result.message;
    return 2;
  }
  std::cout << result.message << "\n";
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc == 3 && std::string(argv[1]) == "generate-4v") {
    try {
      return vc::RunFourViewGenerate(argv[2]);
    } catch (const std::exception& ex) {
      std::cerr << ex.what() << "\n";
      return 1;
    }
  }

  if (argc != 5 || std::string(argv[1]) != "generate-verify") {
    std::cerr << "Usage: " << argv[0]
              << " generate-verify <input_root> <config_path> <output_root>\n"
              << "       " << argv[0] << " generate-4v <config.yaml>\n";
    return 1;
  }

  try {
    return GenerateVerify(argv[2], argv[3], argv[4]);
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << "\n";
    return 1;
  }
}
