idf_component_register(
    SRCS
        "http_server.c"
        "tls_cert.c"
        "auth.c"
        "fw_install.c"
        "record_api.c"
        "audio_api.c"
        "memory_api.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    REQUIRES
        esp_http_server
        esp_https_server
    PRIV_REQUIRES
        video_pipeline
        kvm_screentext
        kvm_hid
        kvm_config
        kvm_storage
        kvm_record
        kvm_audio
        kvm_net
        kvm_atx
        kvm_display
        kvm_log
        kvm_mqtt
        espressif__cjson
        kvm_runbook
        kvm_sched
        kvm_notify
        app_update
        esp_app_format
        espcoredump
        esp_timer
        esp_driver_tsens
        mbedtls
        nvs_flash
        esp_hw_support
        kvm_board
        esp_http_client
        esp-tls
        esp_driver_gpio
    EMBED_FILES
        "assets/index.html.gz"
        "assets/favicon.ico"
        "assets/manifest.webmanifest"
        "assets/sw.js"
        "assets/icon-192.png"
        "assets/icon-512.png"
)

# These diagnostics use Wi-Fi components already selected by the board profile.
if(CONFIG_KVM_HTTP_TX_DIAGNOSTICS AND CONFIG_KVM_WIFI)
    idf_component_optional_requires(PRIVATE esp_wifi)
    if(CONFIG_ESP_HOSTED_HOST_PKT_STATS)
        idf_component_optional_requires(PRIVATE espressif__esp_hosted)
    endif()
endif()

if(CONFIG_KVM_HTTP_TX_DIAGNOSTICS)
    idf_build_set_property(LINK_OPTIONS "-Wl,--wrap=esp_netif_receive" APPEND)
endif()
