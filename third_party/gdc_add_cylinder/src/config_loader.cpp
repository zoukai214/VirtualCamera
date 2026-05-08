#include "../include/config_loader.h"
#include <iostream>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

ConfigLoader::ConfigLoader(const std::string& config_path) {
    config_path_ = fs::absolute(config_path).string();
    config_dir_ = fs::path(config_path_).parent_path().string();
    loadConfig();
}

void ConfigLoader::loadConfig() {
    if (!fs::exists(config_path_)) {
        throw std::runtime_error("配置文件不存在: " + config_path_);
    }
    
    try {
        config_ = YAML::LoadFile(config_path_);
        std::cout << "[OK] 成功加载配置文件: " << config_path_ << std::endl;
    } catch (const YAML::Exception& e) {
        throw std::runtime_error("YAML配置文件解析错误: " + std::string(e.what()));
    }
}

std::string ConfigLoader::resolvePath(const std::string& path) const {
    if (fs::path(path).is_absolute()) {
        return path;
    }
    return (fs::path(config_dir_) / path).string();
}

ConfigLoader::InputConfig ConfigLoader::getInputConfig() const {
    InputConfig input;
    
    // 相机参数文件
    std::string camera_params = config_["input"]["camera_params_file"].as<std::string>();
    input.camera_params_file = resolvePath(camera_params);
    
    // 图像文件
    auto images_node = config_["input"]["images"];
    if (images_node) {
        for (const auto& it : images_node) {
            std::string cam_name = it.first.as<std::string>();
            std::string img_path = it.second.as<std::string>();
            input.images[cam_name] = resolvePath(img_path);
        }
    }
    
    // 图像参数
    auto img_params = config_["input"]["image_params"];
    input.image_params.count = img_params["count"].as<int>();
    input.image_params.image_width = img_params["image_width"].as<int>();
    input.image_params.image_height = img_params["image_height"].as<int>();
    
    return input;
}

ConfigLoader::OutputConfig ConfigLoader::getOutputConfig() const {
    OutputConfig output;
    output.camera_maps_file = resolvePath(config_["output"]["camera_maps_file"].as<std::string>());
    output.stitched_result = resolvePath(config_["output"]["stitched_result"].as<std::string>());
    // 输出格式，默认为"bin"
    if (config_["output"]["output_format"]) {
        output.output_format = config_["output"]["output_format"].as<std::string>();
    } else {
        output.output_format = "bin";
    }
    return output;
}

JuncView4ConfigCommon ConfigLoader::getStitchingConfig() const {
    JuncView4ConfigCommon config;
    
    auto stitching = config_["stitching"];
    config.visual_world_width = stitching["visual_world"]["width"].as<double>();
    config.visual_world_height = stitching["visual_world"]["height"].as<double>();
    config.parallel_range = stitching["fusion"]["parallel_range"].as<double>();
    config.curve_range = stitching["fusion"]["curve_range"].as<double>();
    
    auto angles = stitching["fusion"]["angles"];
    config.fl_fusion_angle = angles["front_left"].as<double>();
    config.fr_fusion_angle = angles["front_right"].as<double>();
    config.rl_fusion_angle = angles["rear_left"].as<double>();
    config.rr_fusion_angle = angles["rear_right"].as<double>();
    
    return config;
}

ConfigLoader::ImageConfig ConfigLoader::getImageConfig() const {
    ImageConfig config;
    
    auto image = config_["image"];
    config.output_size.width = image["output_size"]["width"].as<int>();
    config.output_size.height = image["output_size"]["height"].as<int>();
    
    auto vehicle = image["vehicle"];
    config.vehicle.width = vehicle["width"].as<double>();
    config.vehicle.length = vehicle["length"].as<double>();
    config.vehicle.overhang = vehicle["overhang"].as<double>();
    
    return config;
}

ResultSizeParam ConfigLoader::getResultSizeParam() const {
    ImageConfig img_config = getImageConfig();
    JuncView4ConfigCommon stitching_config = getStitchingConfig();
    
    int result_width = img_config.output_size.width;
    int result_height = img_config.output_size.height;
    
    // 车辆参数转换为米
    double car_width_world = img_config.vehicle.width / 1000.0;
    double car_length_world = img_config.vehicle.length / 1000.0;
    
    // 计算像素距离
    double pixel_dis = stitching_config.visual_world_width / static_cast<double>(result_width);
    
    // 计算车标矩形区域
    Rect car_logo_rect;
    car_logo_rect.width = static_cast<int>(car_width_world / pixel_dis);
    car_logo_rect.height = static_cast<int>(car_length_world / pixel_dis);
    car_logo_rect.x = static_cast<int>((result_width - car_logo_rect.width) / 2);
    car_logo_rect.y = static_cast<int>((result_height - car_logo_rect.height) / 2);
    
    return ResultSizeParam(result_width, result_height, car_logo_rect);
}

ConfigLoader::CylinderConfig ConfigLoader::getCylinderConfig() const {
    CylinderConfig cylinder;
    
    auto cyl_node = config_["cylinder"];
    if (!cyl_node) {
        return cylinder;
    }
    
    cylinder.enabled = cyl_node["enabled"].as<bool>(false);
    cylinder.width = cyl_node["width"].as<int>(768);
    cylinder.height = cyl_node["height"].as<int>(512);
    cylinder.fx = cyl_node["fx"].as<double>(229.18);
    cylinder.fy = cyl_node["fy"].as<double>(229.18);
    cylinder.cx = cyl_node["cx"].as<double>(384.0);
    cylinder.cy = cyl_node["cy"].as<double>(224.0);
    cylinder.radius = cyl_node["radius"].as<double>(10000.0);
    if (cyl_node["output_dir"]) {
        cylinder.output_dir = resolvePath(cyl_node["output_dir"].as<std::string>());
    }
    
    return cylinder;
}

bool ConfigLoader::validateConfig() const {
    try {
        // 检查必要的配置节
        std::vector<std::string> required_sections = {"input", "output", "stitching", "image"};
        for (const auto& section : required_sections) {
            if (!config_[section]) {
                std::cerr << "[ERROR] 缺少必要配置节: " << section << std::endl;
                return false;
            }
        }
        
        // 检查输入文件
        InputConfig input = getInputConfig();
        if (!fs::exists(input.camera_params_file)) {
            std::cerr << "[ERROR] 相机参数文件不存在: " << input.camera_params_file << std::endl;
            return false;
        }
        
        // 检查数值参数
        ImageConfig img_config = getImageConfig();
        if (img_config.output_size.width <= 0 || img_config.output_size.height <= 0) {
            std::cerr << "[ERROR] 输出图像尺寸无效" << std::endl;
            return false;
        }
        
        JuncView4ConfigCommon stitching_config = getStitchingConfig();
        if (stitching_config.visual_world_width <= 0 || stitching_config.visual_world_height <= 0) {
            std::cerr << "[ERROR] 可视世界尺寸无效" << std::endl;
            return false;
        }
        
        std::cout << "[OK] 配置文件验证通过" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] 配置验证失败: " << e.what() << std::endl;
        return false;
    }
}

