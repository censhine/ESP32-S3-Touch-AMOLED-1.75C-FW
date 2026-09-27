idf_component_register(
    SRCS "storage_service.c"
    INCLUDE_DIRS "include"
    REQUIRES fatfs wear_levelling esp_hw_support esp_timer
)
