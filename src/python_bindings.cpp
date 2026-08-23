#include "virtual_camera/pipeline_orchestrator.h"

#include <opencv2/imgcodecs.hpp>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace py = pybind11;

namespace {

vc::PipelineSelection ParseSelection(const std::string& selection) {
  if (selection == "all") {
    return vc::PipelineSelection::kAll;
  }
  if (selection == "undistort") {
    return vc::PipelineSelection::kUndistort;
  }
  if (selection == "virtual_camera") {
    return vc::PipelineSelection::kVirtualCamera;
  }
  throw std::runtime_error("unknown pipeline selection: " + selection);
}

py::dict SourceInputToDict(const vc::SourceCameraInput& input) {
  py::dict item;
  item["camera_id"] = input.camera_id;
  item["image_dir"] = input.image_dir;
  return item;
}

class PyPipelineOrchestrator {
 public:
  PyPipelineOrchestrator(const std::string& config_path,
                         const std::string& dataset_root,
                         const std::string& selection)
      : orchestrator_(config_path, dataset_root, ParseSelection(selection)) {}

  std::vector<py::dict> VirtualSourceInputs() const {
    std::vector<py::dict> inputs;
    for (const auto& input : orchestrator_.VirtualSourceInputs()) {
      inputs.push_back(SourceInputToDict(input));
    }
    return inputs;
  }

  std::vector<py::dict> UndistortSourceInputs() const {
    std::vector<py::dict> inputs;
    for (const auto& input : orchestrator_.UndistortSourceInputs()) {
      inputs.push_back(SourceInputToDict(input));
    }
    return inputs;
  }

  void SaveVirtualCameraArtifacts(const std::string& output_root) const {
    orchestrator_.SaveVirtualCameraArtifacts(output_root);
  }

  void SaveUndistortArtifacts(const std::string& output_root) const {
    orchestrator_.SaveUndistortArtifacts(output_root);
  }

  void ProcessAndSaveVirtualCameraFrame(
      int camera_id, const std::string& image_path,
      const std::string& output_root) const {
    const cv::Mat image = cv::imread(image_path, cv::IMREAD_COLOR);
    if (image.empty()) {
      throw std::runtime_error("failed to read image: " + image_path);
    }
    const std::vector<vc::VirtualCameraFrameResult> results =
        orchestrator_.ProcessVirtualCameraFrame(camera_id, image);
    orchestrator_.SaveVirtualCameraFrameResults(
        results, output_root, std::filesystem::path(image_path).filename().string());
  }

  void ProcessAndSaveUndistortFrame(int camera_id,
                                    const std::string& image_path,
                                    const std::string& output_root) const {
    const cv::Mat image = cv::imread(image_path, cv::IMREAD_COLOR);
    if (image.empty()) {
      throw std::runtime_error("failed to read image: " + image_path);
    }
    const std::vector<vc::UndistortFrameResult> results =
        orchestrator_.ProcessUndistortFrame(camera_id, image);
    orchestrator_.SaveUndistortFrameResults(
        results, output_root, std::filesystem::path(image_path).filename().string());
  }

 private:
  vc::PipelineOrchestrator orchestrator_;
};

}  // namespace

PYBIND11_MODULE(virtual_camera, module) {
  py::class_<PyPipelineOrchestrator>(module, "PipelineOrchestrator")
      .def(py::init<const std::string&, const std::string&, const std::string&>(),
           py::arg("config_path"), py::arg("dataset_root"),
           py::arg("selection") = "all")
      .def("virtual_source_inputs",
           &PyPipelineOrchestrator::VirtualSourceInputs)
      .def("undistort_source_inputs",
           &PyPipelineOrchestrator::UndistortSourceInputs)
      .def("save_virtual_camera_artifacts",
           &PyPipelineOrchestrator::SaveVirtualCameraArtifacts)
      .def("save_undistort_artifacts",
           &PyPipelineOrchestrator::SaveUndistortArtifacts)
      .def("process_and_save_virtual_camera_frame",
           &PyPipelineOrchestrator::ProcessAndSaveVirtualCameraFrame,
           py::arg("camera_id"), py::arg("image_path"), py::arg("output_root"))
      .def("process_and_save_undistort_frame",
           &PyPipelineOrchestrator::ProcessAndSaveUndistortFrame,
           py::arg("camera_id"), py::arg("image_path"), py::arg("output_root"));
}
