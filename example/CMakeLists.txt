cmake_minimum_required(VERSION 3.15)


# This CMakeLists.txt defines the build configuration for the example debugger application and test applications.

set(SOURCES main.cpp)

set(COMPILER_FLAGS
    -pedantic
    -W
    -Wall
    -Wcast-align
    -Wconversion
    -Wformat=2
    -Wnarrowing
    -Wnon-virtual-dtor
    -Wnull-dereference
    -Wold-style-cast
    -Woverloaded-virtual
    -Wshadow
    -Wsign-conversion
    -Wunused
    -Wuseless-cast
)

# 64-bit Debugger executable
add_executable(Debugger64 ${SOURCES})

target_link_libraries(Debugger64 PUBLIC Debug64)

set_target_properties(
    Debugger64
    PROPERTIES CXX_STANDARD 17
               CXX_EXTENSIONS OFF
               RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
               COMPILE_OPTIONS -m64
               LINK_FLAGS -m64
)

target_compile_options(
    Debugger64
    PUBLIC ${COMPILER_FLAGS}
)

# 32-bit Debugger executable
add_executable(Debugger32 ${SOURCES})

target_link_libraries(Debugger32 PUBLIC Debug32)

set_target_properties(
    Debugger32
    PROPERTIES CXX_STANDARD 17
               CXX_EXTENSIONS OFF
               RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
               COMPILE_OPTIONS -m32
               LINK_FLAGS -m32
)

target_compile_options(
    Debugger64
    PUBLIC ${COMPILER_FLAGS}
)

# 64-bit Test application
add_executable(TestApp64 test_app.cpp)
set_target_properties(
    TestApp64
    PROPERTIES CXX_STANDARD 17
               CXX_EXTENSIONS OFF
               COMPILE_OPTIONS "-march=native;-m64"
               LINK_FLAGS "-m64"
               RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
target_link_libraries(TestApp64 -static)

# 32-bit Test executable
add_executable(TestApp32 test_app.cpp)
set_target_properties(
    TestApp32
    PROPERTIES CXX_STANDARD 17
               CXX_EXTENSIONS OFF
               COMPILE_OPTIONS "-m32;-mno-sse;-mno-sse2;-mno-sse3;-mno-sse4.1;-mno-sse4.2;-mno-avx;-mno-avx2"
               LINK_FLAGS "-m32"
               RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
target_link_libraries(TestApp32 -static)

