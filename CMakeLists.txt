cmake_minimum_required (VERSION 3.15)

project(Debug CXX)

add_library(Debug
    Debugger.cpp
    Debugger.hpp
    Proc.cpp
    Proc.hpp
    Process.cpp
    Process.hpp
    Thread.cpp
    Thread.hpp
)

set_target_properties(Debug
PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS OFF
)

target_compile_options(Debug
PUBLIC
    -pedantic
    -W
    -Wall
    -Wcast-align
    -Wconversion
    -Wformat=2
    -Wnon-virtual-dtor
    -Wnull-dereference
    -Wold-style-cast
    -Woverloaded-virtual
    -Wshadow
    -Wsign-conversion
    -Wunused
)


add_executable(Debugger
    main.cpp
)

target_link_libraries(Debugger
PUBLIC
    Debug
)

set_target_properties(Debugger
PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS OFF
)

target_compile_options(Debugger
PUBLIC
    -pedantic
    -W
    -Wall
    -Wcast-align
    -Wconversion
    -Wformat=2
    -Wnon-virtual-dtor
    -Wnull-dereference
    -Wold-style-cast
    -Woverloaded-virtual
    -Wshadow
    -Wsign-conversion
    -Wunused
)

