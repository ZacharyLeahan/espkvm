idf_component_register(
    SRCS "kvm_audio.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_common
    PRIV_REQUIRES esp_driver_i2s esp_driver_gpio esp_timer esp_hw_support
)
