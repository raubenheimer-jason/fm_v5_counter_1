idf_component_register(
    SRCS 
        "app_main.c" 
        "components/rtc/rtc.c"
        "components/fram/fram.c"
    INCLUDE_DIRS 
        "."
)