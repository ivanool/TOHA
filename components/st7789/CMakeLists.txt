idf_component_register(
    SRCS "src/st7789.c"
    INCLUDE_DIRS "include"
    REQUIRES driver
)
set(EXTRA_COMPONENT_DIRS components/st7789)
