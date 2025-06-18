cmake_minimum_required(VERSION 3.10)
project(parallel-ssim)

find_package(OpenCV REQUIRED)

add_executable(parallel-ssim src/main.cpp)

target_link_libraries(parallel-ssim ${OpenCV_LIBS})
target_include_directories(parallel-ssim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${OpenCV_INCLUDE_DIRS})

find_package(PkgConfig REQUIRED)
