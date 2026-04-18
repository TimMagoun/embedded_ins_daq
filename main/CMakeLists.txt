idf_component_register(
    SRCS
        "app_main.cpp"
    INCLUDE_DIRS
        "include"
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Warray-bounds=2
    -Wformat=2
    -Wshadow
    -Wundef
    -Wconversion
    -Wsign-conversion
)
