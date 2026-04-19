idf_component_register(
    SRCS
        "app_main.cpp"
        "state_manager.cpp"
        "validate_config.cpp"
    INCLUDE_DIRS
        "include"
        "interfaces"
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
