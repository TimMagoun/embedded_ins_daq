cmake_minimum_required(VERSION 3.16)

project(embedded_ins_daq_host_tests LANGUAGES CXX)

enable_testing()

add_executable(project_skeleton
    test_project_skeleton.cpp
    ../main/app_main.cpp
)

target_compile_features(project_skeleton PRIVATE cxx_std_17)

target_include_directories(project_skeleton PRIVATE
    ../main/include
)

target_compile_options(project_skeleton PRIVATE
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

add_test(NAME project_skeleton COMMAND $<TARGET_FILE:project_skeleton>)
