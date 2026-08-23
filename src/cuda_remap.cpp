#include "virtual_camera/cuda_remap.h"

#include <opencv2/cudawarping.hpp>
#include <opencv2/imgproc.hpp>

#include <stdexcept>

namespace vc {

void RequireCudaRemapAvailable() {
  if (cv::cuda::getCudaEnabledDeviceCount() <= 0) {
    throw std::runtime_error("CUDA remap requires an available CUDA device");
  }
}

CudaRemapMaps UploadCudaRemapMaps(const cv::Mat& map_x, const cv::Mat& map_y) {
  RequireCudaRemapAvailable();
  if (map_x.empty() || map_y.empty()) {
    throw std::runtime_error("CUDA remap maps must not be empty");
  }
  if (map_x.type() != CV_32FC1 || map_y.type() != CV_32FC1) {
    throw std::runtime_error("CUDA remap maps must be CV_32FC1");
  }
  if (map_x.size() != map_y.size()) {
    throw std::runtime_error("CUDA remap map sizes must match");
  }

  CudaRemapMaps maps;
  maps.map_x.upload(map_x);
  maps.map_y.upload(map_y);
  return maps;
}

cv::Mat GpuRemap(const cv::Mat& image, const CudaRemapMaps& maps) {
  RequireCudaRemapAvailable();
  if (image.empty()) {
    throw std::runtime_error("CUDA remap image must not be empty");
  }
  if (maps.map_x.empty() || maps.map_y.empty()) {
    throw std::runtime_error("CUDA remap maps must be uploaded before remap");
  }

  cv::cuda::GpuMat gpu_input;
  gpu_input.upload(image);

  cv::cuda::GpuMat gpu_output;
  cv::cuda::remap(gpu_input, gpu_output, maps.map_x, maps.map_y,
                  cv::INTER_LINEAR);

  cv::Mat output;
  gpu_output.download(output);
  return output;
}

}  // namespace vc
