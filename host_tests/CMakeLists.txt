cmake_minimum_required(VERSION 3.16)

project(embedded_ins_daq_host_tests LANGUAGES CXX)

option(ENABLE_COVERAGE "Enable gcov-compatible coverage instrumentation" OFF)

include(FetchContent)
include(GoogleTest)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(googletest)

enable_testing()

function(configure_host_test_target target_name)
    target_compile_features(${target_name} PRIVATE cxx_std_17)

    target_include_directories(${target_name} PRIVATE
        ../main/include
        ../main/interfaces
    )

    target_compile_options(${target_name} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Warray-bounds=2
        -Wformat=2
        -Wshadow
        -Wconversion
        -Wsign-conversion
        -Wundef
    )

    if(ENABLE_COVERAGE)
        target_compile_options(${target_name} PRIVATE
            --coverage
            -fprofile-arcs
            -ftest-coverage
        )
        target_link_options(${target_name} PRIVATE
            --coverage
            -fprofile-arcs
            -ftest-coverage
        )
    endif()

    target_link_libraries(${target_name} PRIVATE GTest::gtest_main)
endfunction()

add_executable(project_skeleton
    test_project_skeleton.cpp
    ../main/validate_config.cpp
)

configure_host_test_target(project_skeleton)

gtest_discover_tests(project_skeleton)

add_executable(validate_config_tests
    test_validate_config.cpp
    ../main/validate_config.cpp
)

configure_host_test_target(validate_config_tests)

gtest_discover_tests(validate_config_tests)

add_executable(state_manager_tests
    test_state_manager.cpp
    ../main/state_manager.cpp
)

configure_host_test_target(state_manager_tests)

gtest_discover_tests(state_manager_tests)
