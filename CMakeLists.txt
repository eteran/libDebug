cmake_minimum_required(VERSION 3.10)
project(Debug CXX)

enable_testing()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/Modules/")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(TARGET_ARCH "-m32")

add_subdirectory(lib)
add_subdirectory(example)



