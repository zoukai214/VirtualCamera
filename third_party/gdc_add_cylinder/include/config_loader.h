#ifndef CONFIG_LOADER_H
#define CONFIG_LOADER_H

#include <string>
#include <map>
#include "data_types.h"
#include <yaml-cpp/yaml.h>

class ConfigLoader {
public:
    ConfigLoader(const std::string& config_path);
    
    // 获取输入配置
    struct InputConfig {
        std::string camera_params_file;
        std::map<std::string, std::string> images;  // cam_name -> image_path
        struct ImageParams {
            int count = 4;
            int image_width = 1280;
            int image_height = 800;
        } image_params;
    };
    InputConfig getInputConfig() const;
    
    // 获取输出配置
    struct OutputConfig {
        std::string camera_maps_file;
        std::string stitched_result;
        std::string output_format;  // "bin", "npz", 或 "both"
    };
    OutputConfig getOutputConfig() const;
    
    // 获取拼接配置
    JuncView4ConfigCommon getStitchingConfig() const;
    
    // 获取图像配置
    struct ImageConfig {
        struct OutputSize {
            int width = 640;
            int height = 640;
        } output_size;
        struct Vehicle {
            double width = 1950.0;    // mm
            double length = 4855.0;   // mm
            double overhang = 1068.0; // mm
        } vehicle;
    };
    ImageConfig getImageConfig() const;
    
    // 获取结果尺寸参数
    ResultSizeParam getResultSizeParam() const;
    
    // 获取柱面展开配置
    struct CylinderConfig {
        bool enabled = false;
        int width = 768;
        int height = 512;
        double fx = 229.18;
        double fy = 229.18;
        double cx = 384.0;
        double cy = 224.0;
        double radius = 10000.0;
        std::string output_dir;
    };
    CylinderConfig getCylinderConfig() const;
    
    // 验证配置
    bool validateConfig() const;

private:
    std::string config_path_;
    std::string config_dir_;
    YAML::Node config_;
    
    std::string resolvePath(const std::string& path) const;
    void loadConfig();
};

#endif // CONFIG_LOADER_H

