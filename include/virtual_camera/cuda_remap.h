#pragma once

#include <opencv2/core.hpp>
#include <opencv2/core/cuda.hpp>

namespace vc {

struct CudaRemapMaps {
  cv::cuda::GpuMat map_x;
  cv::cuda::GpuMat map_y;
};

void RequireCudaRemapAvailable();
CudaRemapMaps UploadCudaRemapMaps(const cv::Mat& map_x, const cv::Mat& map_y);
cv::Mat GpuRemap(const cv::Mat& image, const CudaRemapMaps& maps);

}  // namespace vc
