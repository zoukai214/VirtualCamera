cmake_minimum_required(VERSION 3.10)
project(VirtualCamera LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

set(THIRD_PARTY_ROOT ${PROJECT_SOURCE_DIR}/third_party)
set(EIGEN_ROOT ${THIRD_PARTY_ROOT}/eigen)
set(OPENCV_ROOT ${THIRD_PARTY_ROOT}/opencv)
set(NLOHMANN_JSON_ROOT ${THIRD_PARTY_ROOT}/nlohmann_json/include)

find_package(yaml-cpp REQUIRED)

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${EIGEN_ROOT}/include/eigen3)
include_directories(${OPENCV_ROOT}/include)
include_directories(${NLOHMANN_JSON_ROOT})

function(vc_add_imported_static_library target_name archive_path)
    if(NOT EXISTS "${archive_path}")
        message(FATAL_ERROR "Missing vendored OpenCV archive: ${archive_path}")
    endif()
    add_library(${target_name} STATIC IMPORTED GLOBAL)
    set_target_properties(${target_name} PROPERTIES
        IMPORTED_LOCATION "${archive_path}"
    )
endfunction()

vc_add_imported_static_library(vendored_opencv_world ${OPENCV_ROOT}/lib/libopencv_world.a)
vc_add_imported_static_library(vendored_opencv_zlib ${OPENCV_ROOT}/lib/libzlib.a)
vc_add_imported_static_library(vendored_opencv_tegra_hal ${OPENCV_ROOT}/lib/libtegra_hal.a)
vc_add_imported_static_library(vendored_opencv_libjasper ${OPENCV_ROOT}/lib/liblibjasper.a)
vc_add_imported_static_library(vendored_opencv_libjpeg_turbo ${OPENCV_ROOT}/lib/liblibjpeg-turbo.a)
vc_add_imported_static_library(vendored_opencv_libpng ${OPENCV_ROOT}/lib/liblibpng.a)
vc_add_imported_static_library(vendored_opencv_libtiff ${OPENCV_ROOT}/lib/liblibtiff.a)
vc_add_imported_static_library(vendored_opencv_libwebp ${OPENCV_ROOT}/lib/liblibwebp.a)

add_library(vendored_opencv INTERFACE)
target_link_libraries(vendored_opencv INTERFACE
    vendored_opencv_world
    vendored_opencv_zlib
    vendored_opencv_tegra_hal
    vendored_opencv_libjasper
    vendored_opencv_libjpeg_turbo
    vendored_opencv_libpng
    vendored_opencv_libtiff
    vendored_opencv_libwebp
)

set(VIRTUAL_CAMERA_CORE_SOURCES)
list(APPEND VIRTUAL_CAMERA_CORE_SOURCES src/remap_generator.cpp)
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
    src/runtime_args.cpp
)
    if(EXISTS ${PROJECT_SOURCE_DIR}/${source_file})
        list(APPEND VIRTUAL_CAMERA_CORE_SOURCES ${source_file})
    endif()
endforeach()

add_library(virtual_camera_core ${VIRTUAL_CAMERA_CORE_SOURCES})

target_link_libraries(virtual_camera_core PUBLIC
    vendored_opencv
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

if(EXISTS ${PROJECT_SOURCE_DIR}/tests/test_jobs.cpp)
    add_executable(test_jobs tests/test_jobs.cpp)
    target_link_libraries(test_jobs PRIVATE virtual_camera_core)
endif()

if(EXISTS ${PROJECT_SOURCE_DIR}/tests/test_parallel_executor.cpp)
    add_executable(test_parallel_executor tests/test_parallel_executor.cpp)
    target_link_libraries(test_parallel_executor PRIVATE virtual_camera_core)
endif()

if(EXISTS ${PROJECT_SOURCE_DIR}/tests/test_virtual_camera_processor.cpp)
    add_executable(test_virtual_camera_processor tests/test_virtual_camera_processor.cpp)
    target_link_libraries(test_virtual_camera_processor PRIVATE virtual_camera_core)
endif()

if(EXISTS ${PROJECT_SOURCE_DIR}/tests/test_remap_generator.cpp)
    add_executable(test_remap_generator tests/test_remap_generator.cpp)
    target_link_libraries(test_remap_generator PRIVATE virtual_camera_core)
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

if(EXISTS ${PROJECT_SOURCE_DIR}/tests/test_runtime_args.cpp)
    add_executable(test_runtime_args tests/test_runtime_args.cpp)
    target_link_libraries(test_runtime_args PRIVATE virtual_camera_core)
endif()

if(EXISTS ${PROJECT_SOURCE_DIR}/tests/test_rt024_runtime.cpp)
    add_executable(test_rt024_runtime tests/test_rt024_runtime.cpp)
    target_link_libraries(test_rt024_runtime PRIVATE virtual_camera_core)
endif()

if(EXISTS ${PROJECT_SOURCE_DIR}/tests/test_image_virtual_script.cpp)
    add_executable(test_image_virtual_script tests/test_image_virtual_script.cpp)
    target_link_libraries(test_image_virtual_script PRIVATE virtual_camera_core)
endif()

set(RECTIFY_PACKAGE_OUTPUT_X86_LIB_DIR output/x86/lib)

if(TARGET virtual_camera_tool)
    install(TARGETS virtual_camera_tool
        RUNTIME DESTINATION .
    )
endif()

install(FILES
    ${PROJECT_SOURCE_DIR}/configs/config_rt024_parallel_run.json
    DESTINATION .
    RENAME config.json
)

install(PROGRAMS
    ${PROJECT_SOURCE_DIR}/image_virtual.bash
    DESTINATION .
)

install(DIRECTORY
    ${PROJECT_SOURCE_DIR}/third_party/opencv/lib/
    DESTINATION ${RECTIFY_PACKAGE_OUTPUT_X86_LIB_DIR}
    FILES_MATCHING
    PATTERN "*.so"
    PATTERN "*.so.*"
)

install(CODE
    "file(MAKE_DIRECTORY \"\$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/${RECTIFY_PACKAGE_OUTPUT_X86_LIB_DIR}\")"
)
