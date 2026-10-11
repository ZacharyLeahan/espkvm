idf_component_register(
    SRCS "main.c" "lcd_record_control.c" "xbox_ble.c"
    INCLUDE_DIRS "."
    REQUIRES
        video_pipeline
        kvm_net
        kvm_web
        kvm_hid
        kvm_runbook
        kvm_sched
        kvm_notify
        kvm_config
        kvm_log
        kvm_atx
        kvm_mqtt
        nvs_flash
        kvm_storage
        kvm_record
        kvm_display
        app_update
        esp_timer
        espcoredump
)
# Optional dependencies must be attached after registration: Kconfig values
# are not available during IDF's early dependency-expansion pass.
if(CONFIG_KVM_XBOX_BLE)
    idf_component_optional_requires(PRIVATE bt espressif__esp_hosted)
endif()
