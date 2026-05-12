cmake_minimum_required(VERSION 3.10)
project(VirtualCamera LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

set(REF_DEPS_ROOT /workspace/icv_vc_bin_lib/deps/x86)
set(REF_OPENCV_ROOT ${REF_DEPS_ROOT}/opencv)
set(REF_EIGEN_ROOT ${REF_DEPS_ROOT}/eigen3-3.3.9)
set(RT024_MAP_LIB_ROOT /workspace/gen_vc_bin_lib_test/deps/x86/gen_vc_map_lib)

find_package(yaml-cpp REQUIRED)

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/third_party/gdc_add_cylinder/include)
include_directories(${PROJECT_SOURCE_DIR}/third_party/nlohmann_json/include)
include_directories(${REF_EIGEN_ROOT}/include/eigen3)
include_directories(${REF_OPENCV_ROOT}/include)
include_directories(${RT024_MAP_LIB_ROOT}/include)

set(VIRTUAL_CAMERA_CORE_SOURCES)
foreach(source_file
    src/json_utils.cpp
    src/pipeline_config.cpp
    src/calibration_loader.cpp
    src/undistort_processor.cpp
    src/virtual_camera_processor.cpp
    src/task_builder.cpp
    src/map_generator.cpp
    src/json_writer.cpp
    src/verifier.cpp
    src/four_view_runner.cpp
    third_party/gdc_add_cylinder/src/bin_file_io.cpp
    third_party/gdc_add_cylinder/src/camera_maps_generator.cpp
    third_party/gdc_add_cylinder/src/camera_params_loader.cpp
    third_party/gdc_add_cylinder/src/config_loader.cpp
    third_party/gdc_add_cylinder/src/cylinder_map.cpp
    third_party/gdc_add_cylinder/src/data_types.cpp
    third_party/gdc_add_cylinder/src/simple_json_parser.cpp
    third_party/gdc_add_cylinder/src/weight_calculator.cpp
)
    if(EXISTS ${PROJECT_SOURCE_DIR}/${source_file})
        list(APPEND VIRTUAL_CAMERA_CORE_SOURCES ${source_file})
    endif()
endforeach()

add_library(virtual_camera_core ${VIRTUAL_CAMERA_CORE_SOURCES})

target_link_libraries(virtual_camera_core PUBLIC
    ${REF_OPENCV_ROOT}/lib/libopencv_world.a
    ${REF_OPENCV_ROOT}/lib/libzlib.a
    ${REF_OPENCV_ROOT}/lib/libtegra_hal.a
    ${REF_OPENCV_ROOT}/lib/liblibjasper.a
    ${REF_OPENCV_ROOT}/lib/liblibjpeg-turbo.a
    ${REF_OPENCV_ROOT}/lib/liblibpng.a
    ${REF_OPENCV_ROOT}/lib/liblibtiff.a
    ${REF_OPENCV_ROOT}/lib/liblibwebp.a
    ${RT024_MAP_LIB_ROOT}/lib/libgen_vc_map_lib.so.1.0
    ${RT024_MAP_LIB_ROOT}/lib/libgdc_bin_generator.so
    ${RT024_MAP_LIB_ROOT}/lib/libgdc.so
    pthread
    rt
    dl
    yaml-cpp
)

if(EXISTS ${PROJECT_SOURCE_DIR}/src/main.cpp)
    add_executable(virtual_camera_tool src/main.cpp)
    target_link_libraries(virtual_camera_tool PRIVATE virtual_camera_core)
endif()

if(EXISTS ${PROJECT_SOURCE_DIR}/tests/test_task_builder.cpp)
    add_executable(test_task_builder tests/test_task_builder.cpp)
    target_link_libraries(test_task_builder PRIVATE virtual_camera_core)
endif()

if(EXISTS ${PROJECT_SOURCE_DIR}/tests/test_verifier.cpp)
    add_executable(test_verifier tests/test_verifier.cpp)
    target_link_libraries(test_verifier PRIVATE virtual_camera_core)
endif()

if(EXISTS ${PROJECT_SOURCE_DIR}/tests/test_four_view_config.cpp)
    add_executable(test_four_view_config tests/test_four_view_config.cpp)
    target_link_libraries(test_four_view_config PRIVATE virtual_camera_core)
endif()

if(EXISTS ${PROJECT_SOURCE_DIR}/tests/test_four_view_camera_params.cpp)
    add_executable(test_four_view_camera_params tests/test_four_view_camera_params.cpp)
    target_link_libraries(test_four_view_camera_params PRIVATE virtual_camera_core)
endif()

if(EXISTS ${PROJECT_SOURCE_DIR}/tests/test_jobs.cpp)
    add_executable(test_jobs tests/test_jobs.cpp)
    target_link_libraries(test_jobs PRIVATE virtual_camera_core)
endif()

if(EXISTS ${PROJECT_SOURCE_DIR}/tests/test_parallel_executor.cpp)
    add_executable(test_parallel_executor tests/test_parallel_executor.cpp)
    target_link_libraries(test_parallel_executor PRIVATE virtual_camera_core)
endif()

if(EXISTS ${PROJECT_SOURCE_DIR}/tests/test_pipeline_runner.cpp)
    add_executable(test_pipeline_runner tests/test_pipeline_runner.cpp)
    target_link_libraries(test_pipeline_runner PRIVATE virtual_camera_core)
endif()

if(EXISTS ${PROJECT_SOURCE_DIR}/tests/test_pipeline_config.cpp)
    add_executable(test_pipeline_config tests/test_pipeline_config.cpp)
    target_link_libraries(test_pipeline_config PRIVATE virtual_camera_core)
endif()

if(EXISTS ${PROJECT_SOURCE_DIR}/tests/test_calibration_loader.cpp)
    add_executable(test_calibration_loader tests/test_calibration_loader.cpp)
    target_link_libraries(test_calibration_loader PRIVATE virtual_camera_core)
endif()

if(EXISTS ${PROJECT_SOURCE_DIR}/tests/test_undistort_processor.cpp)
    add_executable(test_undistort_processor tests/test_undistort_processor.cpp)
    target_link_libraries(test_undistort_processor PRIVATE virtual_camera_core)
endif()

if(EXISTS ${PROJECT_SOURCE_DIR}/tests/test_rt024_json_writer.cpp)
    add_executable(test_rt024_json_writer tests/test_rt024_json_writer.cpp)
    target_link_libraries(test_rt024_json_writer PRIVATE virtual_camera_core)
endif()

if(EXISTS ${PROJECT_SOURCE_DIR}/tests/test_rt024_verifier.cpp)
    add_executable(test_rt024_verifier tests/test_rt024_verifier.cpp)
    target_link_libraries(test_rt024_verifier PRIVATE virtual_camera_core)
endif()
