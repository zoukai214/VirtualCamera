#include "../include/weight_calculator.h"
#include <cmath>
#include <algorithm>

cv::Point2f WeightCalculator::calculateWeightCoordinate(
    int pixel_x, int pixel_y,
    const Rect& vehicle_logo_rect,
    int region_id
) {
    cv::Point2f weight_pos(0.0f, 0.0f);
    
    if (region_id == 0) {  // 左上角
        weight_pos.x = static_cast<float>(pixel_x - vehicle_logo_rect.x);
        weight_pos.y = static_cast<float>(vehicle_logo_rect.y - pixel_y);
    } else if (region_id == 1) {  // 右上角
        weight_pos.x = static_cast<float>(pixel_x - vehicle_logo_rect.x - vehicle_logo_rect.width);
        weight_pos.y = static_cast<float>(vehicle_logo_rect.y - pixel_y);
    } else if (region_id == 2) {  // 左下角
        weight_pos.x = static_cast<float>(pixel_x - vehicle_logo_rect.x);
        weight_pos.y = static_cast<float>(vehicle_logo_rect.y + vehicle_logo_rect.height - pixel_y);
    } else if (region_id == 3) {  // 右下角
        weight_pos.x = static_cast<float>(pixel_x - vehicle_logo_rect.x - vehicle_logo_rect.width);
        weight_pos.y = static_cast<float>(vehicle_logo_rect.y + vehicle_logo_rect.height - pixel_y);
    }
    
    return weight_pos;
}

float WeightCalculator::calculate4ViewWeightCurve(
    int camera_flag,
    int row, int col,
    cv::Mat& weight_2d,
    const ResultSizeParam& result_param,
    const JuncView4ConfigCommon& common_config,
    double vehicle_width,
    double vehicle_length,
    double vehicle_overhang
) {
    const Rect& car_logo_rect = result_param.car_logo_rect;
    int parallel_range = static_cast<int>(common_config.parallel_range);
    double fl_angle = common_config.fl_fusion_angle;
    double fr_angle = common_config.fr_fusion_angle;
    double rl_angle = common_config.rl_fusion_angle;
    double rr_angle = common_config.rr_fusion_angle;
    
    float weight = 1.0f;
    
    if (camera_flag == 0) {  // 前镜头的图片展开
        if (col < car_logo_rect.x || col > (car_logo_rect.x + car_logo_rect.width)) {
            // 左上角融合区域
            if (col < car_logo_rect.x && row <= car_logo_rect.y) {
                cv::Point2f weight_pos = calculateWeightCoordinate(col, row, car_logo_rect, 0);
                
                // 计算平行线参数
                double angle_rad = (-fl_angle / 180.0) * M_PI;
                double k_ratio = std::abs(angle_rad) > 1e-6 ? std::tan(angle_rad) : 1e-6;
                double sin_temp = std::sin(((90 - fl_angle) / 180.0) * M_PI);
                double y_trans = std::abs(sin_temp) > 1e-6 ? (parallel_range / 2.0) / sin_temp : 1e6;
                
                // 统一使用平行线融合区域计算权重
                double dist = std::abs(weight_pos.y - k_ratio * weight_pos.x + y_trans) 
                            / std::sqrt(k_ratio * k_ratio + 1);
                weight = parallel_range > 1e-6 ? static_cast<float>(dist / parallel_range) : 1e6f;
                weight = std::max(0.0f, std::min(1.0f, weight));
                if (weight_pos.y - k_ratio * weight_pos.x + y_trans <= 0) {
                    weight = 0.0f;
                }
                
                weight_2d.at<float>(row, col) = weight;
            }
            
            // 右上角融合区域
            if (col > car_logo_rect.x + car_logo_rect.width && row <= car_logo_rect.y) {
                cv::Point2f weight_pos = calculateWeightCoordinate(col, row, car_logo_rect, 1);
                
                // 计算平行线参数
                double angle_rad = (fr_angle / 180.0) * M_PI;
                double k_ratio = std::abs(angle_rad) > 1e-6 ? std::tan(angle_rad) : 1e-6;
                double sin_temp = std::sin(((90 - fr_angle) / 180.0) * M_PI);
                double y_trans = std::abs(sin_temp) > 1e-6 ? (parallel_range / 2.0) / sin_temp : 1e6;
                
                // 统一使用平行线融合区域计算权重
                double dist = std::abs(weight_pos.y - k_ratio * weight_pos.x + y_trans) 
                            / std::sqrt(k_ratio * k_ratio + 1);
                weight = parallel_range > 1e-6 ? static_cast<float>(dist / parallel_range) : 1e6f;
                weight = std::max(0.0f, std::min(1.0f, weight));
                if (weight_pos.y - k_ratio * weight_pos.x + y_trans <= 0) {
                    weight = 0.0f;
                }
                
                weight_2d.at<float>(row, col) = weight;
            }
        }
    }
    
    if (camera_flag == 1) {  // 后镜头的图片展开
        if (col < car_logo_rect.x || col > (car_logo_rect.x + car_logo_rect.width)) {
            // 左下角融合区域
            if (col < car_logo_rect.x && row >= car_logo_rect.y + car_logo_rect.height) {
                cv::Point2f weight_pos = calculateWeightCoordinate(col, row, car_logo_rect, 2);
                
                // 计算平行线参数
                double angle_rad = (rl_angle / 180.0) * M_PI;
                double k_ratio = std::abs(angle_rad) > 1e-6 ? std::tan(angle_rad) : 1e-6;
                double sin_temp = std::sin(((90 - rl_angle) / 180.0) * M_PI);
                double y_trans = std::abs(sin_temp) > 1e-6 ? (parallel_range / 2.0) / sin_temp : 1e6;
                
                // 统一使用平行线融合区域计算权重
                double dist = std::abs(weight_pos.y - k_ratio * weight_pos.x - y_trans) 
                            / std::sqrt(k_ratio * k_ratio + 1);
                weight = parallel_range > 1e-6 ? static_cast<float>(dist / parallel_range) : 1e6f;
                weight = std::max(0.0f, std::min(1.0f, weight));
                if (weight_pos.y - k_ratio * weight_pos.x - y_trans >= 0) {
                    weight = 0.0f;
                }
                
                weight_2d.at<float>(row, col) = weight;
            }
            
            // 右下角融合区域
            if (col > (car_logo_rect.x + car_logo_rect.width) && 
                row >= (car_logo_rect.y + car_logo_rect.height)) {
                cv::Point2f weight_pos = calculateWeightCoordinate(col, row, car_logo_rect, 3);
                
                // 计算平行线参数
                double angle_rad = (-rr_angle / 180.0) * M_PI;
                double k_ratio = std::abs(angle_rad) > 1e-6 ? std::tan(angle_rad) : 1e-6;
                double sin_temp = std::sin(((90 - rr_angle) / 180.0) * M_PI);
                double y_trans = std::abs(sin_temp) > 1e-6 ? (parallel_range / 2.0) / sin_temp : 1e6;
                
                // 统一使用平行线融合区域计算权重
                double dist = std::abs(weight_pos.y - k_ratio * weight_pos.x - y_trans) 
                            / std::sqrt(k_ratio * k_ratio + 1);
                weight = parallel_range > 1e-6 ? static_cast<float>(dist / parallel_range) : 1e6f;
                weight = std::max(0.0f, std::min(1.0f, weight));
                if (weight_pos.y - k_ratio * weight_pos.x - y_trans >= 0) {
                    weight = 0.0f;
                }
                
                weight_2d.at<float>(row, col) = weight;
            }
        }
    }
    
    return weight;
}
