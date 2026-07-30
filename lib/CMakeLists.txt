cmake_minimum_required(VERSION 3.15)

set(SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/Breakpoint.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/Context.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/Debugger.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/DebuggerError.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/Elf.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/Event.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/EventStatus.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/Memory.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/Proc.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/Process.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/Ptrace.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/Region.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/RegisterRef.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/ThreadBase.hpp
    Debugger.cpp
    Elf.cpp
    Memory.cpp
    Proc.cpp
    Process.cpp
    ThreadBase.cpp
)

if(TARGET_ARCH_FAMILY_X86)
    set(SOURCES ${SOURCES}
        ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/BreakpointIntel.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/ThreadIntel.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/ContextIntel.hpp
        BreakpointIntel.cpp
        ContextIntel.cpp
        ThreadIntel.cpp
    )
elseif(TARGET_ARCH_FAMILY_ARM)
    set(SOURCES ${SOURCES}
        ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/BreakpointArm.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/ThreadArm.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/Debug/ContextArm.hpp
        BreakpointArm.cpp
        ContextArm.cpp
        ThreadArm.cpp
    )
endif()

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



if(TARGET_ARCH_FAMILY_X86)
    # 64-bit library target
    add_library(Debug64 ${SOURCES})

    target_include_directories(Debug64
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    # Apply target properties and optional coverage flags
    set_target_properties(Debug64
        PROPERTIES CXX_STANDARD 17
                CXX_EXTENSIONS OFF
    )

    # Always apply architecture compile option
    target_compile_options(Debug64
        PRIVATE -m64
        PUBLIC ${COMPILER_FLAGS}
    )

    if(ENABLE_COVERAGE)
        target_compile_options(Debug64 PRIVATE -g -O0 --coverage)
        set_target_properties(Debug64 PROPERTIES LINK_FLAGS "-m64 --coverage")
    else()
        set_target_properties(Debug64 PROPERTIES LINK_FLAGS "-m64")
    endif()

    target_link_libraries(Debug64 GSL)


    if(BUILD_32_BIT)
        # 32-bit library target
        add_library(Debug32 ${SOURCES})

        target_include_directories(Debug32
            PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
        )

        # Apply target properties and optional coverage flags
        set_target_properties(Debug32
            PROPERTIES CXX_STANDARD 17
                    CXX_EXTENSIONS OFF
        )

        # Always apply architecture compile option
        target_compile_options(Debug32
            PRIVATE -m32
            PUBLIC ${COMPILER_FLAGS}
        )

        if(ENABLE_COVERAGE)
            target_compile_options(Debug32 PRIVATE -g -O0 --coverage)
            set_target_properties(Debug32 PROPERTIES LINK_FLAGS "-m32 --coverage")
        else()
            set_target_properties(Debug32 PROPERTIES LINK_FLAGS "-m32")
        endif()

        target_link_libraries(Debug32 GSL)
    endif()
elseif(TARGET_ARCH_FAMILY_ARM)
    # 64-bit library target
    add_library(Debug64 ${SOURCES})

    target_include_directories(Debug64
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    # Apply target properties and optional coverage flags
    set_target_properties(Debug64
        PROPERTIES CXX_STANDARD 17
                CXX_EXTENSIONS OFF
    )

    # Always apply architecture compile option
    target_compile_options(Debug64
        PUBLIC ${COMPILER_FLAGS}
    )

    if(ENABLE_COVERAGE)
        target_compile_options(Debug64 PRIVATE -g -O0 --coverage)
        set_target_properties(Debug64 PROPERTIES LINK_FLAGS "--coverage")
    endif()

    target_link_libraries(Debug64 GSL)
endif()



