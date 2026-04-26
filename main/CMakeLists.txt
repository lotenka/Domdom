idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES light wifi_manager mqtt
)