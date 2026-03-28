cmake_minimum_required(VERSION 3.16)

project(embedded_ins_daq_host_tests C)

include(CTest)

add_executable(host_smoke smoke_host_test.c)
target_include_directories(host_smoke PRIVATE ${CMAKE_SOURCE_DIR}/../main/include)

add_executable(
    clock_host_test
    clock_host_test.c
    ../main/platform/clock_probe.c
)
target_include_directories(clock_host_test PRIVATE ${CMAKE_SOURCE_DIR}/../main/include)

add_executable(
    board_profile_host_test
    board_profile_host_test.c
    ../main/platform/board_profile.c
)
target_include_directories(board_profile_host_test PRIVATE ${CMAKE_SOURCE_DIR}/../main/include)

add_test(NAME host_smoke COMMAND host_smoke)
set_tests_properties(
    host_smoke
    PROPERTIES
    ENVIRONMENT "HOST_TEST_ARTIFACT_DIR=${CMAKE_BINARY_DIR}/artifacts/host_smoke"
)

add_test(NAME clock_host_test COMMAND clock_host_test)
add_test(NAME board_profile_host_test COMMAND board_profile_host_test)
