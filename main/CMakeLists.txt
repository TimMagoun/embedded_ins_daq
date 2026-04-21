idf_component_register(
    SRCS
        "adapters/esp32/esp32_clock_adapter.cpp"
        "app_main.cpp"
        "state_manager.cpp"
        "validate_config.cpp"
    INCLUDE_DIRS
        "include"
        "interfaces"
        "core"
        "adapters/esp32"
    REQUIRES
        driver
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall
    -Wextra
    -Warray-bounds=2
    -Wformat=2
    -Wshadow
    -Wconversion
    -Wsign-conversion
)
