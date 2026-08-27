cmake_minimum_required(VERSION 3.15)

# Version source.
#
# When this repository is built standalone, VERSION is the single source of
# truth. When it is consumed as a submodule of the Reforge desktop application,
# the enclosing Directory.Build.props takes precedence so that the managed and
# native components cannot drift apart.
set(OPENCGM_VERSION_PROPS_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../Directory.Build.props")
if(EXISTS "${OPENCGM_VERSION_PROPS_FILE}")
    file(READ "${OPENCGM_VERSION_PROPS_FILE}" OPENCGM_VERSION_PROPS_CONTENTS)
    string(REGEX MATCH "<VersionPrefix>([^<]+)</VersionPrefix>" _OPENCGM_VERSION_PREFIX_MATCH "${OPENCGM_VERSION_PROPS_CONTENTS}")
    set(OPENCGM_VERSION_PREFIX "${CMAKE_MATCH_1}")
    if(OPENCGM_VERSION_PREFIX STREQUAL "")
        message(FATAL_ERROR "Failed to parse <VersionPrefix> from ${OPENCGM_VERSION_PROPS_FILE}")
    endif()

    string(REGEX MATCH "<VersionSuffix>([^<]+)</VersionSuffix>" _OPENCGM_VERSION_SUFFIX_MATCH "${OPENCGM_VERSION_PROPS_CONTENTS}")
    set(OPENCGM_VERSION_SUFFIX "${CMAKE_MATCH_1}")
    message(STATUS "Engine version from ${OPENCGM_VERSION_PROPS_FILE}: ${OPENCGM_VERSION_PREFIX}")
else()
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" OPENCGM_VERSION_PREFIX LIMIT_COUNT 1)
    string(STRIP "${OPENCGM_VERSION_PREFIX}" OPENCGM_VERSION_PREFIX)
    set(OPENCGM_VERSION_SUFFIX "")
    message(STATUS "Engine version from VERSION file: ${OPENCGM_VERSION_PREFIX}")
endif()

if(OPENCGM_VERSION_SUFFIX STREQUAL "")
    set(OPENCGM_VERSION_SEMVER "${OPENCGM_VERSION_PREFIX}")
else()
    set(OPENCGM_VERSION_SEMVER "${OPENCGM_VERSION_PREFIX}-${OPENCGM_VERSION_SUFFIX}")
endif()

string(REPLACE "." ";" OPENCGM_VERSION_PARTS "${OPENCGM_VERSION_PREFIX}")
list(LENGTH OPENCGM_VERSION_PARTS OPENCGM_VERSION_PARTS_COUNT)
if(NOT OPENCGM_VERSION_PARTS_COUNT EQUAL 3)
    message(FATAL_ERROR "VersionPrefix must be MAJOR.MINOR.PATCH, got: ${OPENCGM_VERSION_PREFIX}")
endif()
list(GET OPENCGM_VERSION_PARTS 0 OPENCGM_VERSION_MAJOR)
list(GET OPENCGM_VERSION_PARTS 1 OPENCGM_VERSION_MINOR)
list(GET OPENCGM_VERSION_PARTS 2 OPENCGM_VERSION_PATCH)

project(opencgm_engine VERSION ${OPENCGM_VERSION_PREFIX} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(OPENCGM_GENERATED_INCLUDE_DIR "${CMAKE_BINARY_DIR}/generated/include")
file(MAKE_DIRECTORY "${OPENCGM_GENERATED_INCLUDE_DIR}/opencgm")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/opencgm_version_config.h.in"
    "${OPENCGM_GENERATED_INCLUDE_DIR}/opencgm/version_config.h"
    @ONLY
)

# Embed the built-in profile preset catalog at build time so the runtime
# fallback can never drift from engine/config/profile-presets.json (the single
# source of truth). Editing the JSON re-triggers configuration.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/config/profile-presets.json" OPENCGM_PROFILE_PRESETS_JSON)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/profile_presets_embedded.h.in"
    "${OPENCGM_GENERATED_INCLUDE_DIR}/opencgm/profile_presets_embedded.h"
    @ONLY
)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/config/profile-presets.json")

# Options
option(BUILD_SHARED_LIBS "Build shared library" ON)
option(BUILD_TESTS "Build tests" ON)
option(BUILD_DEBUG_TOOLS "Build debug and diagnostic tools (development only)" OFF)
option(ENABLE_SKIA_RENDERER "Build with native Skia renderer support" OFF)

# Skia integration (optional)
if(ENABLE_SKIA_RENDERER)
    find_package(unofficial-skia CONFIG)
    if(unofficial-skia_FOUND)
        message(STATUS "Found Skia via vcpkg")
        set(SKIA_AVAILABLE TRUE)
    else()
        # Try to find Skia in a custom location
        if(DEFINED SKIA_DIR)
            set(SKIA_INCLUDE_DIR "${SKIA_DIR}/include")
            set(SKIA_LIBRARY_DIR "${SKIA_DIR}/out/Release-x64")
            if(EXISTS "${SKIA_INCLUDE_DIR}/core/SkCanvas.h")
                message(STATUS "Found Skia in custom location: ${SKIA_DIR}")
                set(SKIA_AVAILABLE TRUE)
            endif()
        endif()

        if(NOT SKIA_AVAILABLE)
            message(WARNING "Skia not found. Native CGM renderer will not be built.\n"
                            "To enable Skia support:\n"
                            "  1. Install vcpkg and run: vcpkg install skia:x64-windows\n"
                            "  2. Configure with: -DCMAKE_TOOLCHAIN_FILE=[vcpkg]/scripts/buildsystems/vcpkg.cmake\n"
                            "  OR set SKIA_DIR to a prebuilt Skia directory")
            set(SKIA_AVAILABLE FALSE)
        endif()
    endif()
endif()

# Include directories
include_directories(${OPENCGM_GENERATED_INCLUDE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Source files
file(GLOB_RECURSE SOURCES
    "src/core/*.cpp"
    "src/classes/*.cpp"
    "src/commands/*.cpp"
    "src/enums/*.cpp"
    "src/import/*.cpp"
    "src/export/*.cpp"
    "src/utils/*.cpp"
    "src/api/*.cpp"
    "src/svg/*.cpp"
    "src/validation/*.cpp"
    "src/nurbs/*.cpp"
    "src/xcf/*.cpp"
)
# Add c_api.cpp directly from src/ directory
list(APPEND SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/c_api.cpp)
list(APPEND SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniz/miniz.c
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniz/miniz_tdef.c
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniz/miniz_tinfl.c
)

# Conditionally add Skia renderer sources
if(SKIA_AVAILABLE)
    file(GLOB SKIA_SOURCES "src/skia/*.cpp")
    list(APPEND SOURCES ${SKIA_SOURCES})
    message(STATUS "Including Skia renderer sources: ${SKIA_SOURCES}")
endif()

file(GLOB_RECURSE HEADERS
    "include/*.h"
    "include/*.hpp"
)

# Library target (SHARED to create DLL with import lib on Windows)
add_library(opencgm_engine SHARED ${SOURCES} ${HEADERS})

target_include_directories(opencgm_engine
    PUBLIC
        $<BUILD_INTERFACE:${OPENCGM_GENERATED_INCLUDE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_compile_definitions(opencgm_engine PRIVATE OPENCGM_BUILD)

set_target_properties(opencgm_engine PROPERTIES
    OUTPUT_NAME "opencgm"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    POSITION_INDEPENDENT_CODE ON
)

# Platform-specific symbol export
if(MSVC)
    set_target_properties(opencgm_engine PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
else()
    # GCC/Clang: export all symbols (matches Windows EXPORT_ALL_SYMBOLS behaviour).
    # cgm_validate and other tools link directly against C++ classes that have no
    # explicit visibility attributes, so hidden visibility breaks them.
    target_compile_definitions(opencgm_engine PRIVATE OPENCGM_SHARED_LIBRARY)
endif()

# Compiler warnings and definitions for opencgm_engine
if(MSVC)
    target_compile_options(opencgm_engine PRIVATE /W4)
    # Suppress C4505 (unreferenced function with internal linkage) for third-party miniz
    target_compile_options(opencgm_engine PRIVATE /wd4505)
    # Suppress CRT security warnings for getenv() - cross-platform code
    target_compile_definitions(opencgm_engine PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(opencgm_engine PRIVATE windowscodecs shlwapi ole32 cabinet)
else()
    target_compile_options(opencgm_engine PRIVATE -Wall -Wextra -Wpedantic)
    # Linux/macOS: link POSIX threading and dynamic loader
    target_link_libraries(opencgm_engine PRIVATE pthread dl)
endif()

# Skia linking for opencgm_engine (if available)
if(SKIA_AVAILABLE)
    target_compile_definitions(opencgm_engine PUBLIC CGM_SKIA_RENDERER_ENABLED)
    if(unofficial-skia_FOUND)
        target_link_libraries(opencgm_engine PRIVATE unofficial::skia::skia)
    else()
        target_include_directories(opencgm_engine PRIVATE ${SKIA_INCLUDE_DIR})
        target_link_directories(opencgm_engine PRIVATE ${SKIA_LIBRARY_DIR})
        target_link_libraries(opencgm_engine PRIVATE skia)
    endif()
    message(STATUS "OpenCGM engine Skia renderer enabled")
endif()

# Install rules
install(TARGETS opencgm_engine
    EXPORT opencgm_engineTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/ DESTINATION include)
install(FILES "${OPENCGM_GENERATED_INCLUDE_DIR}/opencgm/version_config.h" DESTINATION include/opencgm)

# Examples
# add_subdirectory(examples)  # Directory does not exist yet

# Tools - CGM validation utility
add_executable(cgm_validate src/tools/cgm_validate.cpp)
target_link_libraries(cgm_validate PRIVATE opencgm_engine)
target_include_directories(cgm_validate PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(cgm_validate PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

install(TARGETS cgm_validate RUNTIME DESTINATION bin)

# OpenCGM CLI

add_executable(opencgm_cli
    src/tools/opencgm_cli.cpp
)
target_link_libraries(opencgm_cli PRIVATE opencgm_engine)
target_include_directories(opencgm_cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)


set_target_properties(opencgm_cli PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

install(TARGETS opencgm_cli RUNTIME DESTINATION bin)

# Debug and diagnostic tools (development only)
# Enable with: cmake -DBUILD_DEBUG_TOOLS=ON ..
if(BUILD_DEBUG_TOOLS)
    message(STATUS "Building debug and diagnostic tools")

    # Test - Raster conversion utility
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test/raster_sample_conversion.cpp)
        add_executable(raster_sample_conversion test/raster_sample_conversion.cpp)
        target_link_libraries(raster_sample_conversion PRIVATE opencgm_engine)
        target_include_directories(raster_sample_conversion PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
        set_target_properties(raster_sample_conversion PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        )
    endif()

    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test/nurbs_sample_conversion.cpp)
        add_executable(nurbs_sample_conversion test/nurbs_sample_conversion.cpp)
        target_link_libraries(nurbs_sample_conversion PRIVATE opencgm_engine)
        target_include_directories(nurbs_sample_conversion PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
        set_target_properties(nurbs_sample_conversion PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        )
    endif()

    add_executable(symbol_audit src/tools/symbol_audit.cpp)
    target_link_libraries(symbol_audit PRIVATE opencgm_engine)
    target_include_directories(symbol_audit PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    set_target_properties(symbol_audit PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test/cgm_element_inspector.cpp)
        add_executable(cgm_element_inspector test/cgm_element_inspector.cpp)
        target_link_libraries(cgm_element_inspector PRIVATE opencgm_engine)
        target_include_directories(cgm_element_inspector PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
        set_target_properties(cgm_element_inspector PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        )
    endif()

    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test/debug_transform.cpp)
        add_executable(debug_transform test/debug_transform.cpp)
        target_link_libraries(debug_transform PRIVATE opencgm_engine)
        target_include_directories(debug_transform PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
        set_target_properties(debug_transform PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        )
    endif()

    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test/debug_sdr.cpp)
        add_executable(debug_sdr test/debug_sdr.cpp)
        target_link_libraries(debug_sdr PRIVATE opencgm_engine)
        target_include_directories(debug_sdr PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
        set_target_properties(debug_sdr PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        )
    endif()

    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test/nurbs_conversion_test.cpp)
        add_executable(nurbs_conversion_test test/nurbs_conversion_test.cpp)
        target_link_libraries(nurbs_conversion_test PRIVATE opencgm_engine)
        target_include_directories(nurbs_conversion_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
        set_target_properties(nurbs_conversion_test PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        )
    endif()

    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test/nurbs_debug.cpp)
        add_executable(nurbs_debug test/nurbs_debug.cpp)
        target_link_libraries(nurbs_debug PRIVATE opencgm_engine)
        target_include_directories(nurbs_debug PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
        set_target_properties(nurbs_debug PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        )
    endif()
endif() # BUILD_DEBUG_TOOLS

# Tests
if(BUILD_TESTS)
    enable_testing()
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt)
        add_subdirectory(tests)
    else()
        message(WARNING "BUILD_TESTS is ON but tests/CMakeLists.txt not found")
    endif()
endif()
