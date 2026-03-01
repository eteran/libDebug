cmake_minimum_required(VERSION 3.15)

include(CTest)

add_executable(UnitTests
    Test.cpp
    Test.hpp
    unit_tests.cpp
    readwrite_tests.cpp
    process_events_tests.cpp
    clone_exit_tests.cpp
    breakpoint_tests.cpp)


target_link_libraries(UnitTests PUBLIC Debug)
set_target_properties(UnitTests PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# Apply architecture compile option and optional coverage flags
target_compile_options(UnitTests PRIVATE ${TARGET_ARCH})
if(ENABLE_COVERAGE)
    target_compile_options(UnitTests PRIVATE -g -O0 --coverage)
    set(TEST_LINK_FLAGS "${TARGET_ARCH} --coverage")
else()
    set(TEST_LINK_FLAGS "${TARGET_ARCH}")
endif()
set_target_properties(UnitTests PROPERTIES LINK_FLAGS "${TEST_LINK_FLAGS}")

add_test(NAME UnitTests COMMAND UnitTests)
