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

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/third_party/nlohmann_json/include)
include_directories(${REF_EIGEN_ROOT}/include/eigen3)
include_directories(${REF_OPENCV_ROOT}/include)

set(VIRTUAL_CAMERA_CORE_SOURCES)
foreach(source_file
    src/json_utils.cpp
    src/task_builder.cpp
    src/map_generator.cpp
    src/json_writer.cpp
    src/verifier.cpp
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
    pthread
    rt
    dl
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
