#include "virtual_camera/cuda_remap.h"

#include <opencv2/core.hpp>

#include <stdexcept>
#include <string>

namespace {

void Expect(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void TestRequireCudaRemapAvailable() {
  vc::RequireCudaRemapAvailable();
}

void TestGpuRemapIdentityMapKeepsPixels() {
  vc::RequireCudaRemapAvailable();

  cv::Mat image(4, 4, CV_8UC3);
  for (int row = 0; row < image.rows; ++row) {
    for (int col = 0; col < image.cols; ++col) {
      image.at<cv::Vec3b>(row, col) = cv::Vec3b(
          static_cast<unsigned char>(col),
          static_cast<unsigned char>(row),
          static_cast<unsigned char>(row + col));
    }
  }

  cv::Mat map_x(4, 4, CV_32FC1);
  cv::Mat map_y(4, 4, CV_32FC1);
  for (int row = 0; row < map_x.rows; ++row) {
    for (int col = 0; col < map_x.cols; ++col) {
      map_x.at<float>(row, col) = static_cast<float>(col);
      map_y.at<float>(row, col) = static_cast<float>(row);
    }
  }

  const vc::CudaRemapMaps maps = vc::UploadCudaRemapMaps(map_x, map_y);
  const cv::Mat output = vc::GpuRemap(image, maps);

  Expect(output.rows == image.rows && output.cols == image.cols,
         "identity GPU remap should keep image size");
  for (int row = 0; row < image.rows; ++row) {
    for (int col = 0; col < image.cols; ++col) {
      Expect(output.at<cv::Vec3b>(row, col) == image.at<cv::Vec3b>(row, col),
             "identity GPU remap should keep pixels");
    }
  }
}

}  // namespace

int main() {
  TestRequireCudaRemapAvailable();
  TestGpuRemapIdentityMapKeepsPixels();
  return 0;
}
