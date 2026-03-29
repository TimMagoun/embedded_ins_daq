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
    ethernet_smoke_host_test
    ethernet_smoke_host_test.cc
    ../main/platform/ethernet_smoke_config.c
)
target_include_directories(ethernet_smoke_host_test PRIVATE ${CMAKE_SOURCE_DIR}/../main/include)
target_link_libraries(ethernet_smoke_host_test PRIVATE GTest::gtest_main)

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

add_executable(
    sync_capture_service_host_test
    sync_capture_service_host_test.cc
    ../main/control/fault_manager.c
    ../main/logging/binary_log_pipeline.c
    ../main/logging/record_builder.c
    ../main/capture/sync_capture_service.c
)
target_include_directories(sync_capture_service_host_test PRIVATE ${CMAKE_SOURCE_DIR}/../main/include)
target_link_libraries(sync_capture_service_host_test PRIVATE GTest::gtest_main)

add_executable(
    uart_capture_service_host_test
    uart_capture_service_host_test.cc
    ../main/control/fault_manager.c
    ../main/logging/binary_log_pipeline.c
    ../main/logging/record_builder.c
    ../main/capture/uart_capture_service.c
)
target_include_directories(uart_capture_service_host_test PRIVATE ${CMAKE_SOURCE_DIR}/../main/include)
target_link_libraries(uart_capture_service_host_test PRIVATE GTest::gtest_main)

add_executable(
    storage_service_host_test
    storage_service_host_test.cc
    ../main/control/fault_manager.c
    ../main/control/runtime_config.c
    ../main/logging/record_builder.c
    ../main/storage/storage_service.c
)
target_include_directories(storage_service_host_test PRIVATE ${CMAKE_SOURCE_DIR}/../main/include)
target_link_libraries(storage_service_host_test PRIVATE GTest::gtest_main)

add_executable(
    two_port_capture_host_test
    two_port_capture_host_test.cc
    ../main/control/runtime_config.c
    ../main/control/fault_manager.c
    ../main/logging/binary_log_pipeline.c
    ../main/logging/record_builder.c
    ../main/capture/uart_capture_service.c
)
target_include_directories(two_port_capture_host_test PRIVATE ${CMAKE_SOURCE_DIR}/../main/include)
target_link_libraries(two_port_capture_host_test PRIVATE GTest::gtest_main)

add_executable(
    binary_log_pipeline_host_test
    binary_log_pipeline_host_test.cc
    ../main/control/fault_manager.c
    ../main/logging/binary_log_pipeline.c
)
target_include_directories(binary_log_pipeline_host_test PRIVATE ${CMAKE_SOURCE_DIR}/../main/include)
target_link_libraries(binary_log_pipeline_host_test PRIVATE GTest::gtest_main)

add_test(NAME host_smoke COMMAND host_smoke)
set_tests_properties(
    host_smoke
    PROPERTIES
    ENVIRONMENT "HOST_TEST_ARTIFACT_DIR=${CMAKE_BINARY_DIR}/artifacts/host_smoke"
)

include(GoogleTest)
gtest_discover_tests(clock_host_test)
gtest_discover_tests(board_ports_host_test)
gtest_discover_tests(ethernet_smoke_host_test)
gtest_discover_tests(runtime_config_host_test)
gtest_discover_tests(fault_manager_host_test)
gtest_discover_tests(session_controller_host_test)
gtest_discover_tests(record_builder_host_test)
gtest_discover_tests(sync_capture_service_host_test)
gtest_discover_tests(uart_capture_service_host_test)
gtest_discover_tests(storage_service_host_test)
gtest_discover_tests(two_port_capture_host_test)
gtest_discover_tests(binary_log_pipeline_host_test)
