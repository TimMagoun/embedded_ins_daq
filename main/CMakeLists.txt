set(EMBEDDED_INS_DAQ_BUILD_ID "nogit")
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short=12 HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE EMBEDDED_INS_DAQ_BUILD_ID
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()

idf_component_register(
    SRCS
        "app_main.c"
        "platform/board_profile.c"
        "platform/clock_probe.c"
        "platform/clock_service.c"
        "platform/runtime_banner.c"
    INCLUDE_DIRS
        "include"
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    EMBEDDED_INS_DAQ_BUILD_ID="${EMBEDDED_INS_DAQ_BUILD_ID}"
)
