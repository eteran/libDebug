cmake_minimum_required(VERSION 3.15)

add_library(
    Debug
    Breakpoint.cpp
    Context.cpp
    Debugger.cpp
    Proc.cpp
    Process.cpp
    Thread.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/Debugger.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/Event.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/EventStatus.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/RegisterRef.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/Thread.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/Process.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/Region.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/Breakpoint.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/Context.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/DebuggerError.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/Proc.hpp
)

target_include_directories(
    Debug
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(
    Debug
    PROPERTIES CXX_STANDARD 20
               CXX_EXTENSIONS OFF
               COMPILE_OPTIONS ${TARGET_ARCH}
               LINK_FLAGS ${TARGET_ARCH}
)
