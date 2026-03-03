cmake_minimum_required(VERSION 3.15)
project(Debug CXX)

enable_testing()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_COVERAGE "Enable compilation with coverage flags" OFF)
if(ENABLE_COVERAGE)
	message(STATUS "Code coverage instrumentation: ON")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0 --coverage")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 --coverage")
else()
	message(STATUS "Code coverage instrumentation: OFF")
endif()

add_subdirectory(lib)
add_subdirectory(example)
add_subdirectory(tests)



