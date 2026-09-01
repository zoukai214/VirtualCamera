#include "virtual_camera/pipeline_orchestrator.h"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <filesystem>
#include <stdexcept>
#include <string>
#include <unordered_map>
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

  vc::GpuImage ReadImage(const std::string& image_path) const {
    return orchestrator_.ReadImage(image_path);
  }

  void ProcessAndSaveVirtualCameraFrame(
      int camera_id, const std::string& image_path,
      const std::string& output_root) const {
    const vc::GpuImage image = ReadImage(image_path);
    const int result_id = ProcessVirtualCameraFrame(camera_id, image);
    SaveVirtualCameraFrameResults(
        result_id, output_root, std::filesystem::path(image_path).filename().string());
  }

  int ProcessVirtualCameraFrame(int camera_id,
                                const vc::GpuImage& image) const {
    const int result_id = next_result_id_++;
    virtual_results_by_id_[result_id] =
        orchestrator_.ProcessVirtualCameraFrame(camera_id, image);
    return result_id;
  }

  void SaveVirtualCameraFrameResults(int result_id,
                                     const std::string& output_root,
                                     const std::string& input_filename) const {
    const auto found = virtual_results_by_id_.find(result_id);
    if (found == virtual_results_by_id_.end()) {
      throw std::runtime_error("unknown virtual camera result id");
    }
    orchestrator_.SaveVirtualCameraFrameResults(
        found->second, output_root, input_filename);
  }

  void ProcessAndSaveUndistortFrame(int camera_id,
                                    const std::string& image_path,
                                    const std::string& output_root) const {
    const vc::GpuImage image = ReadImage(image_path);
    const int result_id = ProcessUndistortFrame(camera_id, image);
    SaveUndistortFrameResults(
        result_id, output_root, std::filesystem::path(image_path).filename().string());
  }

  int ProcessUndistortFrame(int camera_id, const vc::GpuImage& image) const {
    const int result_id = next_result_id_++;
    undistort_results_by_id_[result_id] =
        orchestrator_.ProcessUndistortFrame(camera_id, image);
    return result_id;
  }

  void SaveUndistortFrameResults(int result_id,
                                 const std::string& output_root,
                                 const std::string& input_filename) const {
    const auto found = undistort_results_by_id_.find(result_id);
    if (found == undistort_results_by_id_.end()) {
      throw std::runtime_error("unknown undistort result id");
    }
    orchestrator_.SaveUndistortFrameResults(
        found->second, output_root, input_filename);
  }

 private:
  vc::PipelineOrchestrator orchestrator_;
  mutable int next_result_id_ = 1;
  mutable std::unordered_map<int, std::vector<vc::VirtualCameraFrameResult>>
      virtual_results_by_id_;
  mutable std::unordered_map<int, std::vector<vc::UndistortFrameResult>>
      undistort_results_by_id_;
};

}  // namespace

PYBIND11_MODULE(virtual_camera, module) {
  py::class_<vc::GpuImage>(module, "GpuImage");

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
      .def("read_image", &PyPipelineOrchestrator::ReadImage,
           py::arg("image_path"))
      .def("process_virtual_camera_frame",
           &PyPipelineOrchestrator::ProcessVirtualCameraFrame,
           py::arg("camera_id"), py::arg("image"))
      .def("save_virtual_camera_frame_results",
           &PyPipelineOrchestrator::SaveVirtualCameraFrameResults,
           py::arg("result_id"), py::arg("output_root"), py::arg("input_filename"))
      .def("process_and_save_virtual_camera_frame",
           &PyPipelineOrchestrator::ProcessAndSaveVirtualCameraFrame,
           py::arg("camera_id"), py::arg("image_path"), py::arg("output_root"))
      .def("process_undistort_frame",
           &PyPipelineOrchestrator::ProcessUndistortFrame,
           py::arg("camera_id"), py::arg("image"))
      .def("save_undistort_frame_results",
           &PyPipelineOrchestrator::SaveUndistortFrameResults,
           py::arg("result_id"), py::arg("output_root"), py::arg("input_filename"))
      .def("process_and_save_undistort_frame",
           &PyPipelineOrchestrator::ProcessAndSaveUndistortFrame,
           py::arg("camera_id"), py::arg("image_path"), py::arg("output_root"));
}
