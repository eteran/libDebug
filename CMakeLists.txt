cmake_minimum_required(VERSION 3.10)
project(Debug CXX)

enable_testing()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/Modules/")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(TARGET_ARCH "-m64")

# Optionally enable code coverage instrumentation. When ON, both the
# library and the tests are compiled/linked with coverage flags so coverage
# tools (gcov/lcov) will record hits for the library code as well.
option(ENABLE_COVERAGE "Enable compilation with coverage flags" OFF)
if(ENABLE_COVERAGE)
	message(STATUS "Code coverage instrumentation: ON")
else()
	message(STATUS "Code coverage instrumentation: OFF")
endif()

if(ENABLE_COVERAGE)
	# Ensure all compilation units get coverage instrumentation and that
	# the final link pulls in libgcov (avoids __gcov_* undefined refs).
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0 --coverage")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 --coverage")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} --coverage")
endif()

add_subdirectory(lib)
add_subdirectory(example)
add_subdirectory(tests)



