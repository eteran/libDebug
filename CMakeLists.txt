cmake_minimum_required(VERSION 3.15)
project(Debug CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_ASAN "Enable address sanitizer")
option(ENABLE_MSAN "Enable memory sanitizer")
option(ENABLE_USAN "Enable undefined sanitizer")
option(ENABLE_TSAN "Enable thread sanitizer")

set(SANITIZERS "")

if(ENABLE_ASAN)
    list(APPEND SANITIZERS "address")
endif()

if(ENABLE_MSAN)
    list(APPEND SANITIZERS "memory")
endif()

if(ENABLE_USAN)
    list(APPEND SANITIZERS "undefined")
endif()

if(ENABLE_TSAN)
    list(APPEND SANITIZERS "thread")
endif()

list(JOIN SANITIZERS "," LIST_OF_SANITIZERS)

if(LIST_OF_SANITIZERS)
    if(NOT "${LIST_OF_SANITIZERS}" STREQUAL "")
        set(CMAKE_CXX_FLAGS
            "${CMAKE_CXX_FLAGS}        -fsanitize=${LIST_OF_SANITIZERS}"
        )
        set(CMAKE_C_FLAGS
            "${CMAKE_C_FLAGS}          -fsanitize=${LIST_OF_SANITIZERS}"
        )
        set(CMAKE_EXE_LINKER_FLAGS
            "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=${LIST_OF_SANITIZERS}"
        )
    endif()
endif()

add_library(
    Debug
    Breakpoint.cpp
    Breakpoint.hpp
    Context.cpp
    Context.hpp
    Debugger.cpp
    Debugger.hpp
    DebuggerError.hpp
    Event.hpp
    EventStatus.hpp
    Proc.cpp
    Proc.hpp
    Process.cpp
    Process.hpp
    Region.hpp
    RegisterRef.hpp
    Thread.cpp
    Thread.hpp
)

set_target_properties(
    Debug PROPERTIES CXX_STANDARD 17 CXX_EXTENSIONS OFF # COMPILE_OPTIONS "-m32"
                     # LINK_FLAGS "-m32"
)

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
    PROPERTIES CXX_STANDARD 17 CXX_EXTENSIONS OFF # COMPILE_OPTIONS "-m32"
               # LINK_FLAGS "-m32"
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
