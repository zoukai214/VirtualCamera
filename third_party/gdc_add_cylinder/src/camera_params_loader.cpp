#include "../include/camera_params_loader.h"
#include "../include/simple_json_parser.h"
#include <iostream>
#include <fstream>
#include <map>
#include <opencv2/opencv.hpp>

bool CameraParamsLoader::loadCameraParams(
    const std::string& json_path,
    int camera_num,
    std::vector<CameraModelExt>& cam_model_ext,
    std::vector<CameraModelInt>& cam_model_int,
    std::vector<CameraModelInt>& cam_model_int_src
) {
    // 初始化相机参数数组
    cam_model_ext.resize(camera_num);
    cam_model_int.resize(camera_num);
    cam_model_int_src.resize(camera_num);
    
    // 读取JSON文件
    std::map<std::string, SimpleJsonParser::JsonValue> config;
    if (!SimpleJsonParser::parseFile(json_path, config)) {
        std::cerr << "Failed to open or parse JSON file: " << json_path << std::endl;
        return false;
    }
    
    // 相机顺序映射(left,front,right,back/rear)
    // JSON文件中使用"back"，但代码中统一使用"rear"作为内部标识
    std::map<std::string, int> cam_map = {
        {"left", 0}, {"front", 1}, {"right", 2}, {"back", 3}, {"rear", 3}
    };
    
    for (const auto& [cam_name, cam_id] : cam_map) {
        if (cam_id >= camera_num) continue;
        
        // 检查相机配置是否存在
        if (config.find("camera_settings") == config.end()) {
            continue;
        }
        auto& camera_settings = config["camera_settings"];
        if (!camera_settings.has(cam_name)) {
            continue;
        }
        
        auto& cam_config = camera_settings[cam_name];
        
        // 读取内参
        auto& intrinsics = cam_config["intrinsics"];
        cam_model_int[cam_id].intrin.resize(9, 0.0);
        cam_model_int[cam_id].intrin[0] = intrinsics[0][0].getDouble();  // fx
        cam_model_int[cam_id].intrin[2] = intrinsics[0][2].getDouble();  // cx
        cam_model_int[cam_id].intrin[4] = intrinsics[1][1].getDouble();  // fy
        cam_model_int[cam_id].intrin[5] = intrinsics[1][2].getDouble();  // cy
        cam_model_int[cam_id].intrin[8] = 1.0;

        auto& intrinsics_src = cam_config["intrinsics_src"];
        cam_model_int_src[cam_id].intrin.resize(9, 0.0);
        cam_model_int_src[cam_id].intrin[0] = intrinsics_src[0][0].getDouble();  // fx
        cam_model_int_src[cam_id].intrin[2] = intrinsics_src[0][2].getDouble();  // cx
        cam_model_int_src[cam_id].intrin[4] = intrinsics_src[1][1].getDouble();  // fy
        cam_model_int_src[cam_id].intrin[5] = intrinsics_src[1][2].getDouble();  // cy
        cam_model_int_src[cam_id].intrin[8] = 1.0;
        
        // 读取畸变系数
        auto& distort = cam_config["distort"];
        cam_model_int[cam_id].distortion_coeff.resize(4);
        cam_model_int_src[cam_id].distortion_coeff.resize(4);
        for (size_t i = 0; i < 4; ++i) {
            cam_model_int[cam_id].distortion_coeff[i] = distort[i].getDouble();
            cam_model_int_src[cam_id].distortion_coeff[i] = distort[i].getDouble();
        }
        
        // 读取外参
        auto& pose_matrix = cam_config["extrinsics"]["pose"];
        
        // 提取旋转矩阵和平移向量
        cv::Mat R = cv::Mat::zeros(3, 3, CV_64F);
        cv::Mat t = cv::Mat::zeros(3, 1, CV_64F);
        
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                R.at<double>(i, j) = pose_matrix[i][j].getDouble();
            }
            t.at<double>(i, 0) = pose_matrix[i][3].getDouble();
        }
        
        cam_model_ext[cam_id].rotation = R.clone();
        cam_model_ext[cam_id].translation = t.clone();
        
        // 计算逆变换
        cv::Mat R_inv = R.inv();
        cv::Mat t_inv = -R_inv * t;
        
        cam_model_ext[cam_id].inv_rotation = R_inv.clone();
        cam_model_ext[cam_id].inv_translation = t_inv.clone();
    }
    
    return true;
}

