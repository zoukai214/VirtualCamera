#ifndef CYLINDER_MAP_H
#define CYLINDER_MAP_H

#include <opencv2/core.hpp>
#include <string>

namespace fisheye_cylinder {

/**
 * @brief 生成柱面映射表
 * 
 * @param k_array 相机内参矩阵 (3x3)
 * @param d_array 畸变系数 (4个元素)
 * @param rt_array 外参矩阵 (4x4, cam2car, 包含旋转和平移)
 * @param camera_id 相机ID (0:前, 1:左, 2:右, 3:后)
 * @param cylinder_width 柱面图宽度
 * @param cylinder_height 柱面图高度
 * @param cylinder_fx 柱面投影焦距x
 * @param cylinder_fy 柱面投影焦距y
 * @param cylinder_cx 柱面投影主点x
 * @param cylinder_cy 柱面投影主点y
 * @param cylinder_radius 柱面半径
 * @param fisheye_width 鱼眼图像宽度（用于越界检查，<=0则不检查）
 * @param fisheye_height 鱼眼图像高度（用于越界检查，<=0则不检查）
 * @param map_x 输出的X方向映射表
 * @param map_y 输出的Y方向映射表
 * @return int 成功返回1，失败返回0
 */
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
                   cv::OutputArray map_y);

/**
 * @brief 将柱面映射表保存为GDC bin文件
 * 
 * bin文件格式: 交织存储的 float (x, y) 对，共 width*height 个像素点
 * 
 * @param map_x X方向映射表
 * @param map_y Y方向映射表
 * @param output_path 输出bin文件路径
 * @return bool 成功返回true
 */
bool saveCylinderMapBin(const cv::Mat& map_x,
                        const cv::Mat& map_y,
                        const std::string& output_path);

}  // namespace fisheye_cylinder

#endif // CYLINDER_MAP_H