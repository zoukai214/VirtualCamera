#include "../include/camera_maps_generator.h"
#include "../include/weight_calculator.h"
#include "virtual_camera/jobs.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <cmath>

struct CameraRegion {
    int cam_id;
    std::string region_name;
    int row_start, row_end, col_start, col_end;
};

CameraMaps CameraMapsGenerator::generateCameraMapsOptimized(
    const ResultSizeParam& result_param,
    const JuncView4ConfigCommon& config_2D,
    const std::vector<CameraModelExt>& cam_model_ext,
    const std::vector<CameraModelInt>& cam_model_int,
    int image_width,
    int image_height,
    double vehicle_width,
    double vehicle_length,
    double vehicle_overhang,
    int jobs
) {
    int result_height = result_param.result_image_height;
    int result_width = result_param.result_image_width;
    const Rect& car_logo_rect = result_param.car_logo_rect;
    
    // 计算像素距离
    double pixel_dis = config_2D.visual_world_width / static_cast<double>(result_width);
    double vehicle_center_offset = (vehicle_length / 2.0 - vehicle_overhang);
    
    // 初始化相机映射表
    CameraMaps camera_maps(result_height, result_width);
    
    // 相机配置：(camera_id, region_name, region_slice)
    std::vector<CameraRegion> camera_configs = {
        {1, "front", 0, car_logo_rect.y - 1, 0, result_width},
        {3, "rear", car_logo_rect.y + car_logo_rect.height - 1, result_height, 0, result_width},
        {0, "left", 0, result_height, 0, car_logo_rect.x -1 },
        {2, "right", 0, result_height, car_logo_rect.x + car_logo_rect.width, result_width}
    };
    
    // 第一步：处理每个相机的坐标映射
    vc::ParallelFor(camera_configs.size(), jobs, [&](std::size_t config_index) {
        const auto& config = camera_configs[config_index];
        std::cout << "处理" << config.region_name << "相机区域坐标映射: "
                  << "行[" << config.row_start << ":" << config.row_end << "], "
                  << "列[" << config.col_start << ":" << config.col_end << "]" << std::endl;
        
        int region_height = config.row_end - config.row_start;
        int region_width = config.col_end - config.col_start;
        
        if (region_height <= 0 || region_width <= 0) {
            return;
        }
        
        // 获取相机参数
        const CameraModelExt& camera_ext = cam_model_ext[config.cam_id];
        const CameraModelInt& camera_int = cam_model_int[config.cam_id];
        
        // 构建相机内参矩阵（使用double类型，OpenCV C++更稳定）
        cv::Mat K = (cv::Mat_<double>(3, 3) <<
            camera_int.intrin[0], 0.0, camera_int.intrin[2],
            0.0, camera_int.intrin[4], camera_int.intrin[5],
            0.0, 0.0, 1.0);
        
        // 构建畸变系数（使用double类型）
        cv::Mat dist = (cv::Mat_<double>(4, 1) <<
            camera_int.distortion_coeff[0],
            camera_int.distortion_coeff[1],
            camera_int.distortion_coeff[2],
            camera_int.distortion_coeff[3]);
        
        // 构建变换矩阵 T_vcs2cam
        cv::Mat T_vcs2cam = cv::Mat::eye(4, 4, CV_64F);
        camera_ext.inv_rotation.copyTo(T_vcs2cam(cv::Rect(0, 0, 3, 3)));
        camera_ext.inv_translation.copyTo(T_vcs2cam(cv::Rect(3, 0, 1, 3)));
        
        // 获取对应的相机映射表
        CameraMap* cam_map = nullptr;
        if (config.region_name == "front") {
            cam_map = &camera_maps.front;
        } else if (config.region_name == "rear") {
            cam_map = &camera_maps.rear;
        } else if (config.region_name == "left") {
            cam_map = &camera_maps.left;
        } else if (config.region_name == "right") {
            cam_map = &camera_maps.right;
        }
        
        if (!cam_map) {
            return;
        }
        
        // 处理区域内的每个像素
        for (int local_i = 0; local_i < region_height; ++local_i) {
            for (int local_j = 0; local_j < region_width; ++local_j) {
                int global_i = config.row_start + local_i;
                int global_j = config.col_start + local_j;
                
                // 计算世界坐标
                double world_x = pixel_dis * (result_height / 2.0 - global_i) + vehicle_center_offset;
                double world_y = pixel_dis * (result_width / 2.0 - global_j);
                double world_z = 0.0;
                
                // 转换为齐次坐标
                cv::Mat point_3d = (cv::Mat_<double>(4, 1) << world_x, world_y, world_z, 1.0);
                
                // 转换到相机坐标系
                cv::Mat point_cam = T_vcs2cam * point_3d;
                cv::Mat point_3d_cam = point_cam(cv::Rect(0, 0, 1, 3));
                
                // 提取3D点坐标
                double x = point_3d_cam.at<double>(0, 0);
                double y = point_3d_cam.at<double>(1, 0);
                double z = point_3d_cam.at<double>(2, 0);
                
                // 使用fisheye投影
                // 准备3D点：使用Point3d格式（与double类型的K和dist一致）
                std::vector<cv::Point3d> object_points;
                object_points.push_back(cv::Point3d(x, y, z));
                
                // rvec和tvec需要是3x1的Mat，使用double类型
                // 参考avm_calibrator.cc的创建方式：直接构造，确保格式正确
                cv::Mat rvec(3, 1, CV_64FC1);
                cv::Mat tvec(3, 1, CV_64FC1);
                // 初始化为0（因为点已经在相机坐标系中）
                rvec.setTo(cv::Scalar(0));
                tvec.setTo(cv::Scalar(0));
                
                // 使用fisheye投影（OpenCV 4.2.0使用旧版本API：objectPoints, imagePoints, rvec, tvec, K, D）
                std::vector<cv::Point2d> image_points;
                cv::fisheye::projectPoints(object_points, image_points, rvec, tvec, K, dist);
                
                float map_x = static_cast<float>(image_points[0].x);
                float map_y = static_cast<float>(image_points[0].y);
                
                // 边界检查（与Python版本完全一致：map_x < 0 或 map_x >= image_width 或 map_y < 0 或 map_y >= image_height 时设为-1）
                // Python: invalid_x = (map_x < 0) | (map_x >= image_width)
                //         invalid_y = (map_y < 0) | (map_y >= image_height)
                //         invalid = invalid_x | invalid_y
                bool invalid_x = (map_x < 0.0f) || (map_x >= static_cast<float>(image_width));
                bool invalid_y = (map_y < 0.0f) || (map_y >= static_cast<float>(image_height));
                bool invalid = invalid_x || invalid_y;
                
                if (!invalid) {
                    cam_map->map_x.at<float>(global_i, global_j) = map_x;
                    cam_map->map_y.at<float>(global_i, global_j) = map_y;
                    cam_map->mask.at<uchar>(global_i, global_j) = 255;
                } else {
                    cam_map->map_x.at<float>(global_i, global_j) = -1.0f;
                    cam_map->map_y.at<float>(global_i, global_j) = -1.0f;
                    cam_map->mask.at<uchar>(global_i, global_j) = 0;
                }
            }
        }
    });
    
    // 第二步：计算前后视图权重
    cv::Mat weight_2_dim = cv::Mat::zeros(result_height, result_width, CV_32F);
    
    for (const auto& config : camera_configs) {
        if (config.region_name != "front" && config.region_name != "rear") {
            continue;
        }
        
        std::cout << "处理" << config.region_name << "相机区域权重: "
                  << "行[" << config.row_start << ":" << config.row_end << "], "
                  << "列[" << config.col_start << ":" << config.col_end << "]" << std::endl;
        
        CameraMap* cam_map = nullptr;
        if (config.region_name == "front") {
            cam_map = &camera_maps.front;
        } else {
            cam_map = &camera_maps.rear;
        }
        
        int region_height = config.row_end - config.row_start;
        int region_width = config.col_end - config.col_start;
        
        for (int local_i = 0; local_i < region_height; ++local_i) {
            for (int local_j = 0; local_j < region_width; ++local_j) {
                int global_i = config.row_start + local_i;
                int global_j = config.col_start + local_j;
                
                if (cam_map->mask.at<uchar>(global_i, global_j) > 0) {
                    int camera_flag = (config.region_name == "front") ? 0 : 1;
                    float weight = WeightCalculator::calculate4ViewWeightCurve(
                        camera_flag, global_i, global_j, weight_2_dim,
                        result_param, config_2D,
                        vehicle_width, vehicle_length, vehicle_overhang
                    );
                    cam_map->weight.at<float>(global_i, global_j) = weight;
                }
            }
        }
    }
    
    // 第三步：计算左右视图权重
    std::vector<CameraRegion> side_weight_configs;
    for (const auto& config : camera_configs) {
        if (config.region_name != "left" && config.region_name != "right") {
            continue;
        }
        side_weight_configs.push_back(config);
    }

    vc::ParallelFor(side_weight_configs.size(), jobs, [&](std::size_t config_index) {
        const auto& config = side_weight_configs[config_index];
        
        std::cout << "处理" << config.region_name << "相机区域权重: "
                  << "行[" << config.row_start << ":" << config.row_end << "], "
                  << "列[" << config.col_start << ":" << config.col_end << "]" << std::endl;
        
        CameraMap* cam_map = nullptr;
        if (config.region_name == "left") {
            cam_map = &camera_maps.left;
        } else {
            cam_map = &camera_maps.right;
        }
        
        int region_height = config.row_end - config.row_start;
        int region_width = config.col_end - config.col_start;
        
        for (int local_i = 0; local_i < region_height; ++local_i) {
            for (int local_j = 0; local_j < region_width; ++local_j) {
                int global_i = config.row_start + local_i;
                int global_j = config.col_start + local_j;
                
                if (cam_map->mask.at<uchar>(global_i, global_j) > 0) {
                    float weight = 1.0f - weight_2_dim.at<float>(global_i, global_j);
                    cam_map->weight.at<float>(global_i, global_j) = weight;
                }
            }
        }
    });
    
    // 第四步：权重归一化处理
    std::cout << "权重归一化处理..." << std::endl;
    
    cv::Mat total_weight = cv::Mat::zeros(result_height, result_width, CV_32F);
    
    // 计算总权重
    std::vector<CameraMap*> all_maps = {
        &camera_maps.front, &camera_maps.rear,
        &camera_maps.left, &camera_maps.right
    };
    
    for (auto* cam_map : all_maps) {
        cv::Mat mask_2d;
        cam_map->mask.convertTo(mask_2d, CV_32F, 1.0 / 255.0);
        cv::Mat weight_2d = cam_map->weight.mul(mask_2d);
        total_weight += weight_2d;
    }
    
    // 归一化各相机权重
    vc::ParallelFor(all_maps.size(), jobs, [&](std::size_t map_index) {
        auto* cam_map = all_maps[map_index];
        cv::Mat mask_2d;
        cam_map->mask.convertTo(mask_2d, CV_32F, 1.0 / 255.0);
        cv::Mat weight_2d = cam_map->weight.mul(mask_2d);
        
        cv::Mat normalized_weight = cv::Mat::zeros(result_height, result_width, CV_32F);
        cv::divide(weight_2d, total_weight, normalized_weight, 1.0, CV_32F);
        
        cam_map->weight = normalized_weight;
    });
    
    // 验证归一化效果
    cv::Mat total_normalized = cv::Mat::zeros(result_height, result_width, CV_32F);
    for (auto* cam_map : all_maps) {
        cv::Mat mask_2d;
        cam_map->mask.convertTo(mask_2d, CV_32F, 1.0 / 255.0);
        cv::Mat weight_2d = cam_map->weight.mul(mask_2d);
        total_normalized += weight_2d;
    }
    
    double max_val;
    cv::minMaxLoc(total_normalized, nullptr, &max_val);
    cv::Mat overlap_mask = total_normalized > 1.01f;
    int overlap_pixels = cv::countNonZero(overlap_mask);
    
    std::cout << "权重归一化验证: 最大总权重=" << max_val 
              << ", 超标像素=" << overlap_pixels << std::endl;
    
    if (overlap_pixels == 0) {
        std::cout << "[OK] 权重归一化成功！" << std::endl;
    } else {
        std::cout << "[WARNING] 权重归一化存在少量误差" << std::endl;
    }
    
    return camera_maps;
}
