cmake_minimum_required(VERSION 3.16)

project(embedded_ins_daq_host_tests C CXX)

include(CTest)
include(FetchContent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(host_smoke smoke_host_test.c)
target_include_directories(host_smoke PRIVATE ${CMAKE_SOURCE_DIR}/../main/include)

add_executable(
    clock_host_test
    clock_host_test.cc
    ../main/platform/clock_probe.c
)
target_include_directories(clock_host_test PRIVATE ${CMAKE_SOURCE_DIR}/../main/include)
target_link_libraries(clock_host_test PRIVATE GTest::gtest_main)

add_executable(
    board_ports_host_test
    board_ports_host_test.cc
    ../main/platform/board_ports.c
)
target_include_directories(board_ports_host_test PRIVATE ${CMAKE_SOURCE_DIR}/../main/include)
target_link_libraries(board_ports_host_test PRIVATE GTest::gtest_main)

add_executable(
    runtime_config_host_test
    runtime_config_host_test.cc
    ../main/platform/board_ports.c
    ../main/control/runtime_config.c
)
target_include_directories(runtime_config_host_test PRIVATE ${CMAKE_SOURCE_DIR}/../main/include)
target_link_libraries(runtime_config_host_test PRIVATE GTest::gtest_main)

add_executable(
    fault_manager_host_test
    fault_manager_host_test.cc
    ../main/control/fault_manager.c
)
target_include_directories(fault_manager_host_test PRIVATE ${CMAKE_SOURCE_DIR}/../main/include)
target_link_libraries(fault_manager_host_test PRIVATE GTest::gtest_main)

add_executable(
    session_controller_host_test
    session_controller_host_test.cc
    ../main/control/runtime_config.c
    ../main/control/fault_manager.c
    ../main/control/session_controller.c
)
target_include_directories(session_controller_host_test PRIVATE ${CMAKE_SOURCE_DIR}/../main/include)
target_link_libraries(session_controller_host_test PRIVATE GTest::gtest_main)

add_executable(
    record_builder_host_test
    record_builder_host_test.cc
    ../main/control/runtime_config.c
    ../main/logging/record_builder.c
)
target_include_directories(record_builder_host_test PRIVATE ${CMAKE_SOURCE_DIR}/../main/include)
target_link_libraries(record_builder_host_test PRIVATE GTest::gtest_main)

add_test(NAME host_smoke COMMAND host_smoke)
set_tests_properties(
    host_smoke
    PROPERTIES
    ENVIRONMENT "HOST_TEST_ARTIFACT_DIR=${CMAKE_BINARY_DIR}/artifacts/host_smoke"
)

include(GoogleTest)
gtest_discover_tests(clock_host_test)
gtest_discover_tests(board_ports_host_test)
gtest_discover_tests(runtime_config_host_test)
gtest_discover_tests(fault_manager_host_test)
gtest_discover_tests(session_controller_host_test)
gtest_discover_tests(record_builder_host_test)
