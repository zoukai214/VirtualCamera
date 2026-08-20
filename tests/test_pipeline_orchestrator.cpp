#include "virtual_camera/pipeline_config.h"
#include "virtual_camera/pipeline_orchestrator.h"

#include <opencv2/imgcodecs.hpp>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {

void Expect(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

vc::NewIntrinsicConfig MakeNewIntrinsic(double fov, int image_width,
                                        int image_height, double focal) {
  vc::NewIntrinsicConfig new_intrinsic;
  new_intrinsic.fov = fov;
  new_intrinsic.focal_u = focal;
  new_intrinsic.center_u = 512.0;
  new_intrinsic.focal_v = focal;
  new_intrinsic.center_v = 256.0;
  new_intrinsic.image_width = image_width;
  new_intrinsic.image_height = image_height;
  new_intrinsic.center = 1;
  return new_intrinsic;
}

std::filesystem::path MakeTestRoot(const std::string& name) {
  const std::filesystem::path root =
      std::filesystem::path("build/test_tmp/pipeline_orchestrator") / name;
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  return root;
}

void WriteTinyCalibrationJson(const std::filesystem::path& path) {
  std::ofstream output(path);
  if (!output.is_open()) {
    throw std::runtime_error("failed to write calibration json: " + path.string());
  }
  output << R"json({
  "camera-front-wide": {
    "param": {
      "cam_matrix": {"data": [[1909.0533447265625,0,1902.6021728515625],[0,1909.345458984375,1088.849609375],[0,0,1]]},
      "cam_dist": {"data": [[0.27403417229652405,-0.018113205209374428,-2.64449499809416e-05,1.5385621736641042e-05,0.0010280556743964553,0.6341700553894043,0,0]]},
      "width": 3840,
      "height": 2160
    }
  },
  "camera-front-wide-to-car": {
    "param": {
      "sensor_calib": {"data": [[-0.010597652684796122,-0.010371027993724968,0.9998900597245306,1.9657248981983317],[-0.9999331002492209,0.004745092656345318,-0.010548891963789553,-0.061965364385173805],[-0.00463516812569198,-0.9999349608219705,-0.010420621018465637,1.5531924098970122],[0,0,0,1]]}
    }
  }
}
)json";
}

void WriteTinySyntheticImage(const std::filesystem::path& path) {
  cv::Mat image(8, 16, CV_8UC3, cv::Scalar(0, 0, 0));
  for (int y = 0; y < image.rows; ++y) {
    for (int x = 0; x < image.cols; ++x) {
      image.at<cv::Vec3b>(y, x) = cv::Vec3b(
          static_cast<unsigned char>(x * 7),
          static_cast<unsigned char>(y * 13),
          static_cast<unsigned char>((x + y) * 5));
    }
  }
  if (!cv::imwrite(path.string(), image)) {
    throw std::runtime_error("failed to write synthetic image: " + path.string());
  }
}

std::filesystem::path MakeDataset(const std::filesystem::path& output_root) {
  const std::filesystem::path dataset_root = output_root / "dataset";
  const std::filesystem::path conf_dir = dataset_root / "calib_extract";
  const std::filesystem::path image_dir = dataset_root / "image_raw" / "front_wide";
  std::filesystem::create_directories(conf_dir);
  std::filesystem::create_directories(image_dir);
  WriteTinyCalibrationJson(conf_dir / "calib_camera_front_wide_to_car.json");
  WriteTinySyntheticImage(image_dir / "synthetic_front_wide.jpg");
  return dataset_root;
}

std::filesystem::path WriteConfigJson(const std::filesystem::path& root) {
  const std::filesystem::path config_path = root / "config.json";
  std::ofstream output(config_path);
  if (!output.is_open()) {
    throw std::runtime_error("failed to write config json: " + config_path.string());
  }
  output << R"json({
  "conf_dir_path": "calib_extract",
  "image_dir_path": "image_raw",
  "vc_image_dir_path": "image_virtual_camera",
  "undistort_image_dir_path": "image_undistortion",
  "vc_conf_dir_path": "calib_virtual_camera",
  "undistort_conf_dir_path": "calib_undistortion",
  "vc_gdcbin_dir_path": "vc_gdcbin_dir_path",
  "showinfo": 0,
  "process_virtual_camera": 1,
  "process_undistort": 1,
  "undistort_image": 0,
  "distort_model": 0,
  "virtual_camera_configs": [
    {
      "desc": "front wide tiny virtual",
      "conf_json": "calib_camera_front_wide_to_car.json",
      "conf_intri_key": "camera-front-wide",
      "conf_extri_key": "camera-front-wide-to-car",
      "image_dir": "front_wide/",
      "save_dir": "front_wide_110/",
      "calib_json": "calib_cam_front_wide_fov110.json",
      "file_prefix": "fw110",
      "vc_mapX_name": "fw110_vc_mapX.bin",
      "vc_mapY_name": "fw110_vc_mapY.bin",
      "src2vc_mapX_name": "fw110_src2vc_mapX.bin",
      "src2vc_mapY_name": "fw110_src2vc_mapY.bin",
      "camera_id": 1,
      "image_width": 3840,
      "image_height": 2160,
      "fov": 120,
      "undistort_image": 1,
      "new_intrinsic": {
        "fov": 110.0,
        "focal_u": 8.0,
        "center_u": 512.0,
        "focal_v": 8.0,
        "center_v": 256.0,
        "image_width": 16,
        "image_height": 8,
        "center": 1
      },
      "new_extrinsics": {
        "pitch": 0.0,
        "roll": 0.0,
        "yaw": 0.0,
        "x": 0.0,
        "y": 0.0,
        "z": 0.0
      }
    }
  ],
  "undistort_configs": [
    {
      "conf_json": "calib_camera_front_wide_to_car.json",
      "intri_key": "camera-front-wide",
      "extri_key": "camera-front-wide-to-car",
      "image_dir": "front_wide/",
      "new_intrinsic": {
        "fov": 110.0,
        "focal_u": 8.0,
        "center_u": 512.0,
        "focal_v": 8.0,
        "center_v": 256.0,
        "image_width": 16,
        "image_height": 8,
        "center": 1
      }
    }
  ],
  "task_parallelism": 1,
  "undistort_parallelism": 1,
  "virtual_camera_parallelism": 1
}
)json";
  return config_path;
}

void TestPipelineOrchestratorBuildsSourceInputsByCameraId() {
  const std::filesystem::path root = MakeTestRoot("source_inputs");
  const std::filesystem::path dataset_root = MakeDataset(root);
  const std::filesystem::path config_path = WriteConfigJson(root);
  const vc::PipelineOrchestrator orchestrator(config_path.string(),
                                             dataset_root.string());

  Expect(orchestrator.UndistortSourceInputs().size() == 1,
         "undistort source inputs should be grouped");
  Expect(orchestrator.UndistortSourceInputs().front().camera_id == 1,
         "undistort camera id should be inferred");
  Expect(orchestrator.VirtualSourceInputs().size() == 1,
         "virtual source inputs should be grouped");
  Expect(orchestrator.VirtualSourceInputs().front().camera_id == 1,
         "virtual camera id should be preserved");
}

void TestPipelineOrchestratorSavesAndProcessesFrames() {
  const std::filesystem::path root = MakeTestRoot("save_process");
  const std::filesystem::path dataset_root = MakeDataset(root);
  const std::filesystem::path config_path = WriteConfigJson(root);
  vc::PipelineOrchestrator orchestrator(config_path.string(), dataset_root.string());

  orchestrator.SaveUndistortArtifacts(root.string());
  orchestrator.SaveVirtualCameraArtifacts(root.string());

  const std::filesystem::path source_image =
      root / "dataset" / "image_raw" / "front_wide" / "synthetic_front_wide.jpg";
  const cv::Mat image = cv::imread(source_image.string(), cv::IMREAD_COLOR);
  Expect(!image.empty(), "fixture image should load");

  orchestrator.ProcessUndistortFrame(1, image, root.string(),
                                     source_image.filename().string());
  orchestrator.ProcessVirtualCameraFrame(1, image, root.string(),
                                         source_image.filename().string());

  Expect(std::filesystem::exists(root / "calib_undistortion" /
                                 "calib_camera_front_wide_to_car.json"),
         "undistort json should be saved");
  Expect(std::filesystem::exists(root / "calib_virtual_camera" /
                                 "calib_cam_front_wide_fov110.json"),
         "virtual json should be saved");
  Expect(std::filesystem::exists(root / "vc_gdcbin_dir_path" /
                                 "fw110_vc_mapX.bin"),
         "virtual map should be saved");
  Expect(std::filesystem::exists(root / "image_undistortion" / "front_wide" /
                                 "synthetic_front_wide.jpg"),
         "undistort frame should be saved");
  Expect(std::filesystem::exists(root / "image_virtual_camera" / "front_wide_110" /
                                 "fw110_synthetic_front_wide.jpg"),
         "virtual frame should be saved");
}

}  // namespace

int main() {
  TestPipelineOrchestratorBuildsSourceInputsByCameraId();
  TestPipelineOrchestratorSavesAndProcessesFrames();
  return 0;
}
