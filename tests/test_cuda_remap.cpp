#include "virtual_camera/cuda_remap.h"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include <filesystem>
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

cv::Mat MakeTestImage() {
  cv::Mat image(4, 4, CV_8UC3);
  for (int row = 0; row < image.rows; ++row) {
    for (int col = 0; col < image.cols; ++col) {
      image.at<cv::Vec3b>(row, col) = cv::Vec3b(
          static_cast<unsigned char>(col),
          static_cast<unsigned char>(row),
          static_cast<unsigned char>(row + col));
    }
  }
  return image;
}

void MakeIdentityMaps(const cv::Size& size, cv::Mat* map_x, cv::Mat* map_y) {
  *map_x = cv::Mat(size, CV_32FC1);
  *map_y = cv::Mat(size, CV_32FC1);
  for (int row = 0; row < size.height; ++row) {
    for (int col = 0; col < size.width; ++col) {
      map_x->at<float>(row, col) = static_cast<float>(col);
      map_y->at<float>(row, col) = static_cast<float>(row);
    }
  }
}

void TestGpuRemapIdentityMapKeepsPixels() {
  vc::RequireCudaRemapAvailable();

  const cv::Mat image = MakeTestImage();

  cv::Mat map_x;
  cv::Mat map_y;
  MakeIdentityMaps(image.size(), &map_x, &map_y);

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

void TestReadImageUploadsToGpu() {
  vc::RequireCudaRemapAvailable();

  const std::filesystem::path root =
      std::filesystem::path("build/test_tmp/cuda_remap");
  std::filesystem::create_directories(root);
  const std::filesystem::path image_path = root / "read_image.jpg";
  Expect(cv::imwrite(image_path.string(), MakeTestImage()),
         "failed to write read image fixture");

  const vc::GpuImage image = vc::ReadImage(image_path.string());

  Expect(!image.image.empty(),
         "read image should upload a non-empty GPU image");
  Expect(image.image.rows == 4 && image.image.cols == 4,
         "GPU image should keep source dimensions");
}

void TestGpuRemapKeepsOutputOnGpu() {
  vc::RequireCudaRemapAvailable();

  const cv::Mat source = MakeTestImage();
  cv::Mat map_x;
  cv::Mat map_y;
  MakeIdentityMaps(source.size(), &map_x, &map_y);
  const vc::CudaRemapMaps maps = vc::UploadCudaRemapMaps(map_x, map_y);

  cv::cuda::GpuMat gpu_source;
  gpu_source.upload(source);
  const cv::cuda::GpuMat gpu_output = vc::GpuRemap(gpu_source, maps);

  Expect(!gpu_output.empty(), "GPU remap output should stay on GPU");
  cv::Mat output;
  gpu_output.download(output);
  Expect(output.size() == source.size(),
         "GPU remap output should keep image size");
  Expect(output.at<cv::Vec3b>(0, 0) == source.at<cv::Vec3b>(0, 0),
         "GPU remap output should preserve identity pixels");
}

void TestReadImageRejectsMissingFile() {
  vc::RequireCudaRemapAvailable();

  bool thrown = false;
  try {
    vc::ReadImage("build/test_tmp/cuda_remap/missing-image.jpg");
  } catch (const std::runtime_error& error) {
    thrown = std::string(error.what()).find("failed to read image") !=
             std::string::npos;
  }
  Expect(thrown, "missing image should throw a read error");
}

}  // namespace

int main() {
  TestRequireCudaRemapAvailable();
  TestGpuRemapIdentityMapKeepsPixels();
  TestReadImageUploadsToGpu();
  TestGpuRemapKeepsOutputOnGpu();
  TestReadImageRejectsMissingFile();
  return 0;
}
