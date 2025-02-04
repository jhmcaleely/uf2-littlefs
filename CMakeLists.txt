cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(uf2-littlefs C CXX ASM)

add_executable(uf2-littlefs main.c littlefs/lfs.c littlefs/lfs_util.c)