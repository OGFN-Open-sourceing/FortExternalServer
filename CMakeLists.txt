cmake_minimum_required(VERSION 3.21)

project(FortExternalServer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows" AND NOT APPLE)
    message(FATAL_ERROR
        "FortExternalServer hosts a Windows or macOS Fortnite build.\n"
        "Build on Windows or macOS, or cross compile a Windows binary with:\n"
        "  cmake --preset mingw-release")
endif()

file(GLOB_RECURSE FORT_EXTERNAL_SERVER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp")

if(APPLE)
    list(FILTER FORT_EXTERNAL_SERVER_SOURCES EXCLUDE REGEX "/Private/Windows/")
else()
    list(FILTER FORT_EXTERNAL_SERVER_SOURCES EXCLUDE REGEX "/Private/Mac/")
endif()

file(GLOB_RECURSE FORT_EXTERNAL_SERVER_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h")

add_executable(FortExternalServer
    ${FORT_EXTERNAL_SERVER_SOURCES}
    ${FORT_EXTERNAL_SERVER_HEADERS})

target_include_directories(FortExternalServer PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source")

if(APPLE)
    target_compile_definitions(FortExternalServer PRIVATE _DARWIN_C_SOURCE)
else()
    target_compile_definitions(FortExternalServer PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        UNICODE
        _UNICODE
        _CRT_SECURE_NO_WARNINGS)
endif()

if(APPLE)
    target_link_libraries(FortExternalServer PRIVATE "-framework CoreFoundation")
    set_target_properties(FortExternalServer PROPERTIES OSX_ARCHITECTURES "x86_64")
else()
    target_link_libraries(FortExternalServer PRIVATE psapi ole32 shlwapi)
endif()

if(APPLE)
    set(FORT_EXTERNAL_SERVER_BINARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Binaries/Mac")
else()
    set(FORT_EXTERNAL_SERVER_BINARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Binaries/Win64")
endif()

set_target_properties(FortExternalServer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${FORT_EXTERNAL_SERVER_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${FORT_EXTERNAL_SERVER_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${FORT_EXTERNAL_SERVER_BINARY_DIR}")

if(MSVC)
    target_compile_options(FortExternalServer PRIVATE /W4 /permissive- /Zc:preprocessor /MP)
    target_link_options(FortExternalServer PRIVATE /SUBSYSTEM:CONSOLE)
elseif(APPLE)
    target_compile_options(FortExternalServer PRIVATE -Wall -Wextra -Wno-unused-parameter)
else()
    target_compile_options(FortExternalServer PRIVATE -Wall -Wextra -Wno-unused-parameter -municode)
    target_link_options(FortExternalServer PRIVATE -municode -static-libgcc -static-libstdc++)
endif()

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/Source" FILES
    ${FORT_EXTERNAL_SERVER_SOURCES}
    ${FORT_EXTERNAL_SERVER_HEADERS})

install(TARGETS FortExternalServer RUNTIME DESTINATION .)
