#include "../include/cylinder_map.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

#include <opencv2/core.hpp>

namespace fisheye_cylinder {

namespace {

void fisheyeProjectPoints(
    const std::vector<cv::Point3f>& object_points,
    std::vector<cv::Point2f>* image_points,
    const double r_body_to_sensor[3][3],
    const double t_body_to_sensor[3],
    const double k_array[3][3],
    const double d_array[4]) {
  const double fx = k_array[0][0];
  const double fy = k_array[1][1];
  const double cx = k_array[0][2];
  const double cy = k_array[1][2];

  image_points->clear();
  image_points->reserve(object_points.size());

  for (const auto& point : object_points) {
    const double wx = point.x;
    const double wy = point.y;
    const double wz = point.z;

    const double ncx = r_body_to_sensor[0][0] * wx +
                       r_body_to_sensor[0][1] * wy +
                       r_body_to_sensor[0][2] * wz + t_body_to_sensor[0];
    const double ncy = r_body_to_sensor[1][0] * wx +
                       r_body_to_sensor[1][1] * wy +
                       r_body_to_sensor[1][2] * wz + t_body_to_sensor[1];
    const double ncz = r_body_to_sensor[2][0] * wx +
                       r_body_to_sensor[2][1] * wy +
                       r_body_to_sensor[2][2] * wz + t_body_to_sensor[2];

    const double r2 = ncx * ncx + ncy * ncy;
    const double r = std::sqrt(r2);
    const double radius = std::sqrt(r2 + ncz * ncz);

    const double theta = std::acos(ncz / radius);
    const double theta2 = theta * theta;
    const double theta3 = theta * theta2;
    const double theta5 = theta3 * theta2;
    const double theta7 = theta5 * theta2;
    const double theta9 = theta7 * theta2;
    const double theta_d =
        theta + d_array[0] * theta3 + d_array[1] * theta5 +
        d_array[2] * theta7 + d_array[3] * theta9;

    const double inv_r = r > 1e-8 ? 1.0 / r : 1.0;
    const double cdist = r > 1e-8 ? theta_d * inv_r : 1.0;
    const double xd0 = ncx * cdist;
    const double yd0 = ncy * cdist;

    image_points->emplace_back(
        static_cast<float>(xd0 * fx + cx),
        static_cast<float>(yd0 * fy + cy));
  }
}

}  // namespace

int genCylinderMap(const double k_array[3][3],
                   const double d_array[4],
                   const double rt_array[4][4],
                   int camera_id,
                   int cylinder_width,
                   int cylinder_height,
                   double cylinder_fx,
                   double cylinder_fy,
                   double cylinder_cx,
                   double cylinder_cy,
                   double cylinder_radius,
                   int fisheye_width,
                   int fisheye_height,
                   cv::OutputArray map_x,
                   cv::OutputArray map_y) {
  std::vector<cv::Point3f> cylinder_points;
  cylinder_points.reserve(
      static_cast<size_t>(cylinder_width) * static_cast<size_t>(cylinder_height));

  for (int row = 0; row < cylinder_height; ++row) {
    for (int col = 0; col < cylinder_width; ++col) {
      const float vp_x = static_cast<float>((col - cylinder_cx) / cylinder_fx);
      const float vp_y = static_cast<float>((row - cylinder_cy) / cylinder_fy);

      cv::Point3f real_point;
      switch (camera_id) {
        case 0:  // front
          real_point.x = static_cast<float>(
              cylinder_radius * std::cos(vp_x) + rt_array[0][3]);
          real_point.y = static_cast<float>(
              -cylinder_radius * std::sin(vp_x) + rt_array[1][3]);
          break;
        case 1:  // left
          real_point.x = static_cast<float>(
              cylinder_radius * std::sin(vp_x) + rt_array[0][3]);
          real_point.y = static_cast<float>(
              cylinder_radius * std::cos(vp_x) + rt_array[1][3]);
          break;
        case 2:  // right
          real_point.x = static_cast<float>(
              -cylinder_radius * std::sin(vp_x) + rt_array[0][3]);
          real_point.y = static_cast<float>(
              -cylinder_radius * std::cos(vp_x) + rt_array[1][3]);
          break;
        case 3:  // rear
          real_point.x = static_cast<float>(
              -cylinder_radius * std::cos(vp_x) + rt_array[0][3]);
          real_point.y = static_cast<float>(
              cylinder_radius * std::sin(vp_x) + rt_array[1][3]);
          break;
        default:
          throw std::invalid_argument("camera_id must be in [0, 3]");
      }

      real_point.z = static_cast<float>(
          -cylinder_radius * vp_y + rt_array[2][3]);
      cylinder_points.push_back(real_point);
    }
  }

  double r_body_to_sensor[3][3];
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      r_body_to_sensor[i][j] = rt_array[j][i];
    }
  }

  const double tx = rt_array[0][3];
  const double ty = rt_array[1][3];
  const double tz = rt_array[2][3];
  const double t_body_to_sensor[3] = {
      -(rt_array[0][0] * tx + rt_array[1][0] * ty + rt_array[2][0] * tz),
      -(rt_array[0][1] * tx + rt_array[1][1] * ty + rt_array[2][1] * tz),
      -(rt_array[0][2] * tx + rt_array[1][2] * ty + rt_array[2][2] * tz),
  };

  std::vector<cv::Point2f> projected_points;
  fisheyeProjectPoints(
      cylinder_points,
      &projected_points,
      r_body_to_sensor,
      t_body_to_sensor,
      k_array,
      d_array);

  map_x.create(cv::Size(cylinder_width, cylinder_height), CV_32FC1);
  map_y.create(cv::Size(cylinder_width, cylinder_height), CV_32FC1);
  cv::Mat map1 = map_x.getMat();
  cv::Mat map2 = map_y.getMat();

  const bool check_bounds = (fisheye_width > 0 && fisheye_height > 0);
  int index = 0;
  for (int i = 0; i < cylinder_height; ++i) {
    for (int j = 0; j < cylinder_width; ++j) {
      const cv::Point2f& pt = projected_points[index];
      if (check_bounds &&
          (pt.x < 0 || pt.x >= (fisheye_width - 1) ||
           pt.y < 0 || pt.y >= (fisheye_height - 1))) {
        map1.at<float>(i, j) = 0.0f;
        map2.at<float>(i, j) = 0.0f;
      } else {
        map1.at<float>(i, j) = pt.x;
        map2.at<float>(i, j) = pt.y;
      }
      ++index;
    }
  }

  return 1;
}

bool saveCylinderMapBin(const cv::Mat& map_x,
                        const cv::Mat& map_y,
                        const std::string& output_path) {
  const int height = map_x.rows;
  const int width = map_x.cols;
  const int size = width * height;

  cv::Mat map_xy = cv::Mat::zeros(size, 1, CV_32FC2);
  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < width; ++j) {
      const int idx = i * width + j;
      const float x = map_x.at<float>(i, j);
      const float y = map_y.at<float>(i, j);
      if (x == 0.0f && y == 0.0f) {
        map_xy.at<cv::Vec2f>(idx, 0) = cv::Vec2f(0.0f, 0.0f);
      } else {
        map_xy.at<cv::Vec2f>(idx, 0) = cv::Vec2f(x, y);
      }
    }
  }

  std::ofstream ofs(output_path, std::ios::binary);
  if (!ofs) {
    std::cerr << "[ERROR] 无法打开文件: " << output_path << std::endl;
    return false;
  }
  ofs.write(reinterpret_cast<const char*>(map_xy.data),
            size * sizeof(float) * 2);
  ofs.flush();
  ofs.close();

  std::cout << "  柱面映射bin已保存: " << output_path << std::endl;
  return true;
}

}  // namespace fisheye_cylinder
