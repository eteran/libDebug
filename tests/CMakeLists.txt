cmake_minimum_required(VERSION 3.15)

include(CTest)

set(SOURCES
    Test.cpp
    Test.hpp
    unit_tests.cpp
    readwrite_tests.cpp
    process_events_tests.cpp
    clone_exit_tests.cpp
    breakpoint_tests.cpp
    sse_state_tests.cpp
)


# 64-bit unit test executable
add_executable(UnitTests64 ${SOURCES})

target_link_libraries(UnitTests64 PUBLIC Debug64)
set_target_properties(UnitTests64 PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# Apply architecture compile option and optional coverage flags
target_compile_options(UnitTests64 PRIVATE -m64)
if(ENABLE_COVERAGE)
    target_compile_options(UnitTests64 PRIVATE -g -O0 --coverage)
    set_target_properties(UnitTests64 PROPERTIES LINK_FLAGS "-m64 --coverage")
else()
    set_target_properties(UnitTests64 PROPERTIES LINK_FLAGS "-m64")
endif()

add_test(NAME UnitTests64 COMMAND UnitTests64)


# 32-bit unit test executable
add_executable(UnitTests32 ${SOURCES})

target_link_libraries(UnitTests32 PUBLIC Debug32)
set_target_properties(UnitTests32 PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# Apply architecture compile option and optional coverage flags
target_compile_options(UnitTests32 PRIVATE -m32)
if(ENABLE_COVERAGE)
    target_compile_options(UnitTests32 PRIVATE -g -O0 --coverage)
    set_target_properties(UnitTests32 PROPERTIES LINK_FLAGS "-m32 --coverage")
else()
    set_target_properties(UnitTests32 PROPERTIES LINK_FLAGS "-m32")
endif()

add_test(NAME UnitTests32 COMMAND UnitTests32)
