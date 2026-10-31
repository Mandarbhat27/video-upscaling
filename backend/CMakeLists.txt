cmake_minimum_required(VERSION 3.16)
project(AdaptiveVideoUpscaling VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optimization flags for performance
if(MSVC)
    add_compile_options(/O2 /W4 /utf-8)
else()
    add_compile_options(-O3 -Wall -Wextra -pedantic)
endif()

# Find OpenMP for parallel computing
find_package(OpenMP REQUIRED)

# Find OpenCV
if(NOT DEFINED OpenCV_DIR AND DEFINED ENV{OpenCV_DIR})
    set(OpenCV_DIR $ENV{OpenCV_DIR})
endif()
if(NOT DEFINED OpenCV_DIR)
    set(OpenCV_DIR "C:/Users/Mandar/tools/opencv-mingw/x64/mingw/lib")
endif()

find_package(OpenCV REQUIRED PATHS ${OpenCV_DIR} NO_DEFAULT_PATH)

message(STATUS "OpenCV found version: ${OpenCV_VERSION}")
message(STATUS "OpenCV library directory: ${OpenCV_DIR}")
message(STATUS "OpenMP CXX flags: ${OpenMP_CXX_FLAGS}")

# Include headers
set(CORE_INCLUDE_DIRS
    ${PROJECT_SOURCE_DIR}/include
    "C:/Users/Mandar/tools/opencv-mingw/include"
)

# Core library source files
set(CORE_SOURCES
    src/interpolation.cpp
    src/metrics.cpp
    src/adaptive_engine.cpp
    src/video_processor.cpp
    src/webcam_processor.cpp
    src/benchmark.cpp
)

# Core Static/Shared library
add_library(adaptive_core STATIC ${CORE_SOURCES})
target_include_directories(adaptive_core PUBLIC ${CORE_INCLUDE_DIRS})
target_link_libraries(adaptive_core PUBLIC
    opencv_core
    opencv_imgproc
    opencv_highgui
    opencv_videoio
    opencv_imgcodecs
    OpenMP::OpenMP_CXX
)

# Main CLI executable
add_executable(adaptive_upscaler src/main.cpp)
target_link_libraries(adaptive_upscaler PRIVATE adaptive_core)

# Tests executable
enable_testing()
add_executable(test_suite ../tests/test_interpolation.cpp)
target_link_libraries(test_suite PRIVATE adaptive_core)
add_test(NAME AllTests COMMAND test_suite)
