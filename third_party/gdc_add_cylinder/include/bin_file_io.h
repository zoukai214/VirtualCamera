#ifndef BIN_FILE_IO_H
#define BIN_FILE_IO_H

#include "data_types.h"
#include <string>

class BinFileIO {
public:
    // 保存相机映射表到bin文件（按相机分别保存，与Python脚本格式兼容）
    // 保存格式：每个相机单独保存为 {cam_name}_map_x.bin, {cam_name}_map_y.bin, 
    // {cam_name}_weight.bin, {cam_name}_mask.bin，并保存metadata.txt
    static bool saveCameraMaps(
        const CameraMaps& camera_maps,
        const std::string& output_dir  // 输出目录，不再是文件名前缀
    );
    
    // 从bin文件目录加载相机映射表
    static bool loadCameraMaps(
        CameraMaps& camera_maps,
        const std::string& maps_dir,  // 映射表目录
        int result_height,
        int result_width
    );
};

#endif // BIN_FILE_IO_H

