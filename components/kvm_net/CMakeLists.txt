# WiFi (via an ESP32-C6 co-processor) is compiled only where a board carries one.
# wifi.c has an #else stub so it always builds; the esp_wifi_remote / esp_wifi
# requirement is added only when CONFIG_KVM_WIFI, so the Waveshare build neither
# links nor needs those components.
set(wifi_reqs "")
if(CONFIG_KVM_WIFI)
    list(APPEND wifi_reqs espressif__esp_wifi_remote espressif__esp_hosted esp_wifi)
endif()

# esp-hosted's unaligned allocator ignores its own PSRAM preference. Only the
# P4 SDIO path has been checked for cache-aligned external DMA buffers.
set(wifi_dma_src "")
if(CONFIG_KVM_WIFI AND CONFIG_IDF_TARGET_ESP32P4 AND
   CONFIG_ESP_HOSTED_HOST_TRANSPORT_BUS_SDIO AND CONFIG_EH_HOST_PORT_DMA_PREFER_SPIRAM)
    set(wifi_dma_src "wifi_dma.c")
endif()

idf_component_register(
    SRCS "ethernet.c"
         "ipv6.c"
         "kvm_ts.c"
         "kvm_wg.c"
         "wifi.c"
         ${wifi_dma_src}
    INCLUDE_DIRS "include"
    # esp_netif is public rather than private: kvm_ipv6.h names esp_netif_t, so
    # anything including it needs esp_netif's headers too.
    REQUIRES esp_netif
    PRIV_REQUIRES
        kvm_config
        kvm_notify
        kvm_storage
        esp_eth
        esp_event
        espressif__mdns
        microlink
        wireguard_lwip
        mbedtls
        lwip
        ${wifi_reqs}
)

# esp-hosted's eager boot-time init (which would claim the P4's single SD host
# controller, shared with the microSD) is disabled per board via
# CONFIG_ESP_HOSTED_AUTO_CALL_INIT_BEFORE_APP_MAIN=n; wifi.c brings the co-processor
# up itself only in a WiFi mode. No linker wrap is needed. See wifi.c.

# ethernet.c resets the EMAC on the way into every restart (esp-idf #19085).
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_restart_noos")

if(wifi_dma_src)
    # esp-hosted's transport archive follows kvm_net, so force extraction before
    # the later archive introduces its wrapped references.
    target_link_libraries(${COMPONENT_LIB} INTERFACE
        "-Wl,--wrap=eh_host_port_dma_alloc" "-Wl,--undefined=__wrap_eh_host_port_dma_alloc")
endif()
