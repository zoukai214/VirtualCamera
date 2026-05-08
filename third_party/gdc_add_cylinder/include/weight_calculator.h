#ifndef WEIGHT_CALCULATOR_H
#define WEIGHT_CALCULATOR_H

#include "data_types.h"
#include <opencv2/opencv.hpp>

class WeightCalculator {
public:
    // 计算权重坐标点
    static cv::Point2f calculateWeightCoordinate(
        int pixel_x, int pixel_y,
        const Rect& vehicle_logo_rect,
        int region_id  // 0:左上角, 1:右上角, 2:左下角, 3:右下角
    );
    
    // 计算四视图权重曲线
    static float calculate4ViewWeightCurve(
        int camera_flag,  // 0:前镜头, 1:后镜头
        int row, int col,
        cv::Mat& weight_2d,
        const ResultSizeParam& result_param,
        const JuncView4ConfigCommon& common_config,
        double vehicle_width,
        double vehicle_length,
        double vehicle_overhang
    );
};

#endif // WEIGHT_CALCULATOR_H

