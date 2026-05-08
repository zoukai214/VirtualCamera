#include "../include/bin_file_io.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <filesystem>
#include <vector>
#include <sstream>

namespace fs = std::filesystem;

// Bin文件格式（与Python脚本兼容）：
// 每个相机单独保存为：
//   - {cam_name}_map_x.bin: height*width个float32
//   - {cam_name}_map_y.bin: height*width个float32
//   - {cam_name}_weight.bin: height*width个float32
//   - {cam_name}_mask.bin: height*width个uint8
// 并保存metadata.txt包含width, height, cameras信息

bool BinFileIO::saveCameraMaps(
    const CameraMaps& camera_maps,
    const std::string& output_dir
) {
    // 创建输出目录
    try {
        fs::create_directories(output_dir);
    } catch (const fs::filesystem_error& e) {
        std::cerr << "无法创建输出目录: " << output_dir << " - " << e.what() << std::endl;
        return false;
    }
    
    int height = camera_maps.front.map_x.rows;
    int width = camera_maps.front.map_x.cols;
    
    // 相机名称列表
    std::vector<std::string> cam_names = {"front", "rear", "left", "right"};
    
    // 相机映射表指针列表
    std::vector<const CameraMap*> cam_maps = {
        &camera_maps.front, &camera_maps.rear,
        &camera_maps.left, &camera_maps.right
    };
    
    // 为每个相机保存映射数据
    for (size_t i = 0; i < cam_names.size(); ++i) {
        const std::string& cam_name = cam_names[i];
        const CameraMap& cam_map = *cam_maps[i];
        
        std::cout << "处理" << cam_name << "相机映射表..." << std::endl;
        
        // 确保数据是连续的
        cv::Mat map_x_cont = cam_map.map_x.clone();
        cv::Mat map_y_cont = cam_map.map_y.clone();
        cv::Mat weight_cont = cam_map.weight.clone();
        cv::Mat mask_cont = cam_map.mask.clone();
        
        // 保存map_x
        std::string map_x_file = (fs::path(output_dir) / (cam_name + "_map_x.bin")).string();
        std::ofstream map_x_out(map_x_file, std::ios::binary);
        if (!map_x_out.is_open()) {
            std::cerr << "无法打开文件进行写入: " << map_x_file << std::endl;
            return false;
        }
        map_x_out.write(reinterpret_cast<const char*>(map_x_cont.data), 
                       height * width * sizeof(float));
        map_x_out.close();
        
        // 保存map_y
        std::string map_y_file = (fs::path(output_dir) / (cam_name + "_map_y.bin")).string();
        std::ofstream map_y_out(map_y_file, std::ios::binary);
        if (!map_y_out.is_open()) {
            std::cerr << "无法打开文件进行写入: " << map_y_file << std::endl;
            return false;
        }
        map_y_out.write(reinterpret_cast<const char*>(map_y_cont.data), 
                       height * width * sizeof(float));
        map_y_out.close();
        
        // 保存weight
        std::string weight_file = (fs::path(output_dir) / (cam_name + "_weight.bin")).string();
        std::ofstream weight_out(weight_file, std::ios::binary);
        if (!weight_out.is_open()) {
            std::cerr << "无法打开文件进行写入: " << weight_file << std::endl;
            return false;
        }
        weight_out.write(reinterpret_cast<const char*>(weight_cont.data), 
                        height * width * sizeof(float));
        weight_out.close();
        
        // 保存mask
        std::string mask_file = (fs::path(output_dir) / (cam_name + "_mask.bin")).string();
        std::ofstream mask_out(mask_file, std::ios::binary);
        if (!mask_out.is_open()) {
            std::cerr << "无法打开文件进行写入: " << mask_file << std::endl;
            return false;
        }
        mask_out.write(reinterpret_cast<const char*>(mask_cont.data), 
                      height * width * sizeof(uchar));
        mask_out.close();
        
        std::cout << "  [OK] " << cam_name << " - map_x: " << height << "x" << width 
                  << ", map_y: " << height << "x" << width << std::endl;
        std::cout << "     weight: " << height << "x" << width 
                  << ", mask: " << height << "x" << width << std::endl;
    }
    
    // 保存元数据信息
    std::string metadata_file = (fs::path(output_dir) / "metadata.txt").string();
    std::ofstream metadata_out(metadata_file);
    if (!metadata_out.is_open()) {
        std::cerr << "无法打开元数据文件进行写入: " << metadata_file << std::endl;
        return false;
    }
    
    metadata_out << "width=" << width << "\n";
    metadata_out << "height=" << height << "\n";
    metadata_out << "cameras=";
    for (size_t i = 0; i < cam_names.size(); ++i) {
        if (i > 0) metadata_out << ",";
        metadata_out << cam_names[i];
    }
    metadata_out << "\n";
    metadata_out.close();
    
    std::cout << "[OK] 转换完成！二进制文件已保存到: " << output_dir << std::endl;
    std::cout << "图像尺寸: " << width << " x " << height << std::endl;
    
    return true;
}

bool BinFileIO::loadCameraMaps(
    CameraMaps& camera_maps,
    const std::string& maps_dir,
    int result_height,
    int result_width
) {
    std::cout << "从目录加载映射表: " << maps_dir << std::endl;
    
    // 读取元数据
    std::string metadata_file = (fs::path(maps_dir) / "metadata.txt").string();
    std::ifstream meta_file(metadata_file);
    if (!meta_file.is_open()) {
        std::cerr << "[ERROR] 无法打开元数据文件: " << metadata_file << std::endl;
        return false;
    }
    
    int width = 0, height = 0;
    std::string line;
    while (std::getline(meta_file, line)) {
        if (line.find("width=") == 0) {
            width = std::stoi(line.substr(6));
        } else if (line.find("height=") == 0) {
            height = std::stoi(line.substr(7));
        }
    }
    meta_file.close();
    
    if (width != result_width || height != result_height) {
        std::cerr << "[ERROR] 映射表尺寸不匹配: " << width << "x" << height 
                  << " vs " << result_width << "x" << result_height << std::endl;
        return false;
    }
    
    // 初始化相机映射表
    camera_maps = CameraMaps(height, width);
    
    // 相机名称列表
    std::vector<std::string> cam_names = {"front", "rear", "left", "right"};
    std::vector<CameraMap*> cam_maps = {
        &camera_maps.front, &camera_maps.rear,
        &camera_maps.left, &camera_maps.right
    };
    
    // 加载每个相机的映射数据
    for (size_t i = 0; i < cam_names.size(); ++i) {
        const std::string& cam_name = cam_names[i];
        CameraMap* maps = cam_maps[i];
        
        std::cout << "加载" << cam_name << "相机映射表..." << std::endl;
        
        // 加载map_x
        std::string map_x_file = (fs::path(maps_dir) / (cam_name + "_map_x.bin")).string();
        std::ifstream map_x_in(map_x_file, std::ios::binary);
        if (!map_x_in.is_open()) {
            std::cerr << "[ERROR] 无法打开文件: " << map_x_file << std::endl;
            return false;
        }
        map_x_in.read(reinterpret_cast<char*>(maps->map_x.data), 
                     height * width * sizeof(float));
        map_x_in.close();
        
        // 加载map_y
        std::string map_y_file = (fs::path(maps_dir) / (cam_name + "_map_y.bin")).string();
        std::ifstream map_y_in(map_y_file, std::ios::binary);
        if (!map_y_in.is_open()) {
            std::cerr << "[ERROR] 无法打开文件: " << map_y_file << std::endl;
            return false;
        }
        map_y_in.read(reinterpret_cast<char*>(maps->map_y.data), 
                     height * width * sizeof(float));
        map_y_in.close();
        
        // 加载weight
        std::string weight_file = (fs::path(maps_dir) / (cam_name + "_weight.bin")).string();
        std::ifstream weight_in(weight_file, std::ios::binary);
        if (!weight_in.is_open()) {
            std::cerr << "[ERROR] 无法打开文件: " << weight_file << std::endl;
            return false;
        }
        weight_in.read(reinterpret_cast<char*>(maps->weight.data), 
                      height * width * sizeof(float));
        weight_in.close();
        
        // 加载mask
        std::string mask_file = (fs::path(maps_dir) / (cam_name + "_mask.bin")).string();
        std::ifstream mask_in(mask_file, std::ios::binary);
        if (!mask_in.is_open()) {
            std::cerr << "[ERROR] 无法打开文件: " << mask_file << std::endl;
            return false;
        }
        mask_in.read(reinterpret_cast<char*>(maps->mask.data), 
                    height * width * sizeof(uchar));
        mask_in.close();
        
        std::cout << "  [OK] " << cam_name << "相机映射表加载完成" << std::endl;
    }
    
    std::cout << "[OK] 所有相机映射表加载完成" << std::endl;
    return true;
}

