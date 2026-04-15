cmake_minimum_required(VERSION 3.15)

set(GSL_CXX_STANDARD "17" CACHE STRING "Use c++ standard")
add_subdirectory(GSL-4.2.1 EXCLUDE_FROM_ALL)
