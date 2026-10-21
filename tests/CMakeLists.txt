# Test CMakeLists.txt
cmake_minimum_required(VERSION 3.15)

# Download and configure Google Test
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)

# For Windows: make GoogleTest use the same C runtime as the engine. Forcing the
# shared CRT under OPENCGM_STATIC_MSVC_RUNTIME would mix /MD and /MT and fail
# to link.
if(OPENCGM_STATIC_MSVC_RUNTIME)
    set(gtest_force_shared_crt OFF CACHE BOOL "" FORCE)
else()
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
endif()
# GoogleTest is a test dependency only; keep it out of `cmake --install`, which
# is how release archives are staged.
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

# Test executable - includes all test files
add_executable(opencgm_tests
    test_cell_array.cpp
    test_cli_and_messages.cpp
    test_nurbs_approximator.cpp
    test_tile_raster.cpp
    test_color_scaling.cpp
    test_color_resolver.cpp
    test_binary_reader.cpp
    test_command_factory.cpp
    test_c_api.cpp
    test_binary_roundtrip.cpp
    test_binary_file_roundtrip.cpp
    test_tile_compression.cpp
    test_parameterized_suite.cpp
    test_pattern_geometry.cpp
    test_conversion_artifacts.cpp
    test_aps_attribute_interpreter.cpp
    test_document_serializer.cpp
    test_aps_geometry.cpp
    test_aps_serializer.cpp
    test_aps_text_decoder.cpp
    test_output_compactor.cpp
    test_png_encoder.cpp
    test_metafile_encoding.cpp
    test_svg_semantic_contracts.cpp
    test_svg_fills.cpp
    test_svg_text_and_arcs.cpp
    test_svg_viewbox.cpp
    test_svg_xml_utils.cpp
    test_tile_geometry.cpp
)

target_link_libraries(opencgm_tests
    PRIVATE
        opencgm_engine
        GTest::gtest_main
)

target_include_directories(opencgm_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party
)

# Ensure test executable can find DLLs on Windows
if(WIN32)
    add_custom_command(TARGET opencgm_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:opencgm_engine>
            $<TARGET_FILE_DIR:opencgm_tests>
    )
endif()

include(GoogleTest)
gtest_discover_tests(opencgm_tests)

# Simple CGM conversion utility for testing
add_executable(convert_cgm convert_cgm.cpp)
target_link_libraries(convert_cgm PRIVATE opencgm_engine)
target_include_directories(convert_cgm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)

if(WIN32)
    add_custom_command(TARGET convert_cgm POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:opencgm_engine>
            $<TARGET_FILE_DIR:convert_cgm>
    )
endif()
