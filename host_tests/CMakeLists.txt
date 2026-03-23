cmake_minimum_required(VERSION 3.16)

project(embedded_ins_daq_host_tests C)

include(CTest)

add_executable(host_smoke smoke_host_test.c)

add_test(NAME host_smoke COMMAND host_smoke)
set_tests_properties(
    host_smoke
    PROPERTIES
    ENVIRONMENT "HOST_TEST_ARTIFACT_DIR=${CMAKE_BINARY_DIR}/artifacts/host_smoke"
)
