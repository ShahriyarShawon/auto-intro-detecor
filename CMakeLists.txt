cmake_minimum_required(VERSION 3.10)
project(parallel-msssim)

add_executable(parallel-msssim src/main.cpp)

target_include_directories(parallel-msssim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

find_package(PkgConfig REQUIRED)
