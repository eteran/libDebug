cmake_minimum_required(VERSION 3.15)

add_library(Debug
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/Breakpoint.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/BreakpointIntel.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/Context.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/Debugger.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/DebuggerError.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/Event.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/EventStatus.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/Proc.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/Process.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/Region.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/RegisterRef.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/ThreadIntel.hpp
    BreakpointIntel.cpp
    ContextIntel.cpp
    Debugger.cpp
    Proc.cpp
    Process.cpp
    ThreadIntel.cpp
)

target_include_directories(Debug
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Apply target properties and optional coverage flags
set_target_properties(Debug
    PROPERTIES CXX_STANDARD 17
               CXX_EXTENSIONS OFF
)

# Always apply architecture compile option
target_compile_options(Debug
    PRIVATE ${TARGET_ARCH}
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
           -Wuseless-cast
)

if(ENABLE_COVERAGE)
    target_compile_options(Debug PRIVATE -g -O0 --coverage)
    set(LIB_LINK_FLAGS "${TARGET_ARCH} --coverage")
else()
    set(LIB_LINK_FLAGS "${TARGET_ARCH}")
endif()
set_target_properties(Debug PROPERTIES LINK_FLAGS "${LIB_LINK_FLAGS}")

