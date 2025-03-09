cmake_minimum_required(VERSION 3.20.3)
project(tspp VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:__cplusplus")

if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MT")
endif ()

add_compile_definitions($<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>)
add_compile_definitions(V8_COMPRESS_POINTERS)
add_compile_definitions(V8_ENABLE_SANDBOX)
add_compile_definitions(V8_ENABLE_CHECKS)
add_compile_definitions(BIND_DATATYPE_USERDATA_SIZE=64)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

set(TSPP_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include CACHE STRING "TSPP Include Dir")

set (all_sources "")
macro(add_sources group dir)
    set(_all "")

    file(GLOB _inc "include/tspp/${dir}/*.h" "include/tspp/${dir}/*.hpp" "include/compile/${dir}/*.inl")
    list(APPEND _all ${_inc})

    file(GLOB _src "src/${dir}/*.cpp" "src/${dir}/*.c")
    list(APPEND _all ${_src})

    list(APPEND all_sources ${_all})
    string(REPLACE "/" "\\" group_path "${group}")
    source_group(${group_path} FILES ${_all})

    unset(_inc)
    unset(_src)
    unset(_all)
endmacro(add_sources)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
if (NOT TARGET bind)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/deps/bind)
endif()

add_sources("src"              ".")
add_sources("src/utils"        "utils")
add_sources("src/builtin"      "builtin")
add_sources("src/modules"      "modules")
add_sources("src/systems"      "systems")
add_sources("src/interfaces"   "interfaces")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include_directories(
    ${TSN_BIND_INCLUDE_DIR}
    ${TSN_UTILS_INCLUDE_DIR}
    ${FFI_INCLUDE_DIR}
    ${TSPP_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/v8/include
)

add_library(tspp ${all_sources})
set_property(TARGET tspp PROPERTY CXX_STANDARD 20)
set_property(TARGET tspp PROPERTY CXX_STANDARD_REQUIRED ON)
target_link_directories(tspp PUBLIC "./deps/v8/lib")
target_link_directories(tspp PUBLIC "./lib")
target_link_libraries(tspp
    bind
    v8_monolith
    WinMM
)

add_subdirectory("./test")