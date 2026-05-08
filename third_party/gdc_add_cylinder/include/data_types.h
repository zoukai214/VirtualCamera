#ifndef DATA_TYPES_H
#define DATA_TYPES_H

#include <vector>
#include <opencv2/opencv.hpp>

// 矩形区域
struct Rect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    
    Rect() = default;
    Rect(int x, int y, int w, int h) : x(x), y(y), width(w), height(h) {}
};

// 相机内参
struct CameraModelInt {
    std::vector<double> intrin;          // 9个元素的内参矩阵（按行展开）
    std::vector<double> distortion_coeff; // 4个元素的畸变系数
    
    CameraModelInt() {
        intrin.resize(9, 0.0);
        distortion_coeff.resize(4, 0.0);
    }
};

// 相机外参
struct CameraModelExt {
    cv::Mat rotation;          // 3x3 旋转矩阵
    cv::Mat translation;       // 3x1 平移向量
    cv::Mat inv_rotation;      // 3x3 逆旋转矩阵
    cv::Mat inv_translation;   // 3x1 逆平移向量
    
    CameraModelExt() {
        rotation = cv::Mat::eye(3, 3, CV_64F);
        translation = cv::Mat::zeros(3, 1, CV_64F);
        inv_rotation = cv::Mat::eye(3, 3, CV_64F);
        inv_translation = cv::Mat::zeros(3, 1, CV_64F);
    }
};

// 结果图像尺寸参数
struct ResultSizeParam {
    int result_image_width;
    int result_image_height;
    Rect car_logo_rect;
    
    ResultSizeParam() = default;
    ResultSizeParam(int w, int h, const Rect& rect) 
        : result_image_width(w), result_image_height(h), car_logo_rect(rect) {}
};

// 四视图拼接配置
struct JuncView4ConfigCommon {
    double visual_world_width = 0.0;
    double visual_world_height = 0.0;
    double parallel_range = 0.0;
    double curve_range = 0.0;
    double fl_fusion_angle = 0.0;  // front-left
    double fr_fusion_angle = 0.0;  // front-right
    double rl_fusion_angle = 0.0;  // rear-left
    double rr_fusion_angle = 0.0;  // rear-right
};

// 相机映射表结构
struct CameraMap {
    cv::Mat map_x;      // 浮点型映射表x坐标
    cv::Mat map_y;      // 浮点型映射表y坐标
    cv::Mat weight;     // 权重表
    cv::Mat mask;       // 掩码表
    
    CameraMap() = default;
    CameraMap(int height, int width) {
        map_x = cv::Mat::ones(height, width, CV_32F) * -1.0f;
        map_y = cv::Mat::ones(height, width, CV_32F) * -1.0f;
        weight = cv::Mat::zeros(height, width, CV_32F);
        mask = cv::Mat::zeros(height, width, CV_8U);
    }
};

// 相机映射表集合
struct CameraMaps {
    CameraMap front;
    CameraMap rear;
    CameraMap left;
    CameraMap right;
    
    CameraMaps() = default;
    CameraMaps(int height, int width) 
        : front(height, width), rear(height, width), 
          left(height, width), right(height, width) {}
};

#endif // DATA_TYPES_H

