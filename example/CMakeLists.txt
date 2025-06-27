cmake_minimum_required(VERSION 3.15)

target_compile_options(
    Debug
    PUBLIC -pedantic
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
)

add_executable(Debugger main.cpp)

target_link_libraries(Debugger PUBLIC Debug)

set_target_properties(
    Debugger
    PROPERTIES CXX_STANDARD 17
               CXX_EXTENSIONS OFF
               RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
               COMPILE_OPTIONS ${TARGET_ARCH}
               LINK_FLAGS ${TARGET_ARCH}
)

target_compile_options(
    Debugger
    PUBLIC -pedantic
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
)

add_executable(TestApp64 test_app.cpp)
set_target_properties(
    TestApp64
    PROPERTIES CXX_STANDARD 17
               CXX_EXTENSIONS OFF
               COMPILE_OPTIONS "-m64"
               LINK_FLAGS "-m64"
               RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
target_link_libraries(TestApp64 -static)

add_executable(TestApp32 test_app.cpp)
set_target_properties(
    TestApp32
    PROPERTIES CXX_STANDARD 17
               CXX_EXTENSIONS OFF
               COMPILE_OPTIONS "-m32"
               LINK_FLAGS "-m32"
               RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
target_link_libraries(TestApp32 -static)
