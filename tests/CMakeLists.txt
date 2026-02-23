cmake_minimum_required(VERSION 3.10)

add_executable(UnitTests
    Test.cpp
    Test.hpp
    unit_tests.cpp
    readwrite_tests.cpp
    breakpoint_tests.cpp)

target_link_libraries(UnitTests PUBLIC Debug)
set_target_properties(
    UnitTests
    PROPERTIES CXX_STANDARD 17
               CXX_EXTENSIONS OFF
               RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
               COMPILE_OPTIONS ${TARGET_ARCH}
               LINK_FLAGS ${TARGET_ARCH}
)

add_test(NAME UnitTests COMMAND UnitTests)
