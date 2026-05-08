cmake_minimum_required(VERSION 3.10)
project(VirtualCamera LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Eigen3 REQUIRED)
find_package(OpenCV REQUIRED COMPONENTS core imgproc calib3d imgcodecs)

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/third_party/nlohmann_json/include)

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

target_link_libraries(virtual_camera_core PUBLIC Eigen3::Eigen ${OpenCV_LIBS})

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
