cmake_minimum_required(VERSION 4.1)
project(tefpkg_tool)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)

add_executable(tefpkg_tool main.cpp
        libs/compressor.c
        libs/lz4hc.c
        libs/lz4.c
        libs/siphash.c
        libs/tefpkg.c
        tool.cpp
)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O3 -DNDEBUG)

    if(NOT MSVC)  # 非Windows平台
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -s")
        endif()
    endif()
endif()