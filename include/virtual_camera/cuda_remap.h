#pragma once

#include <opencv2/core.hpp>
#include <opencv2/core/cuda.hpp>

#include <string>

namespace vc {

struct CudaRemapMaps {
  cv::cuda::GpuMat map_x;
  cv::cuda::GpuMat map_y;
};

struct GpuImage {
  cv::cuda::GpuMat image;
};

void RequireCudaRemapAvailable();
GpuImage ReadImage(const std::string& image_path);
CudaRemapMaps UploadCudaRemapMaps(const cv::Mat& map_x, const cv::Mat& map_y);
cv::cuda::GpuMat GpuRemap(const cv::cuda::GpuMat& image,
                          const CudaRemapMaps& maps);
cv::Mat GpuRemap(const cv::Mat& image, const CudaRemapMaps& maps);

}  // namespace vc
