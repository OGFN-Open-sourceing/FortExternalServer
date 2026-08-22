cmake_minimum_required(VERSION 3.21)

project(FortExternalServer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT WIN32)
    message(FATAL_ERROR "FortExternalServer targets Windows x64 only.")
endif()

file(GLOB_RECURSE FORT_EXTERNAL_SERVER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp")

file(GLOB_RECURSE FORT_EXTERNAL_SERVER_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h")

add_executable(FortExternalServer
    ${FORT_EXTERNAL_SERVER_SOURCES}
    ${FORT_EXTERNAL_SERVER_HEADERS})

target_include_directories(FortExternalServer PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source")

target_compile_definitions(FortExternalServer PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
    _CRT_SECURE_NO_WARNINGS)

target_link_libraries(FortExternalServer PRIVATE psapi)

set_target_properties(FortExternalServer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Binaries/Win64"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/Binaries/Win64"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/Binaries/Win64")

if(MSVC)
    target_compile_options(FortExternalServer PRIVATE /W4 /permissive- /Zc:preprocessor /MP)
    target_link_options(FortExternalServer PRIVATE /SUBSYSTEM:CONSOLE)
else()
    target_compile_options(FortExternalServer PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/Source" FILES
    ${FORT_EXTERNAL_SERVER_SOURCES}
    ${FORT_EXTERNAL_SERVER_HEADERS})
