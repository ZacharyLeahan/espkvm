set(audio_deps)
if(CONFIG_KVM_ENABLE_AUDIO)
    list(APPEND audio_deps espressif__esp_audio_codec)
endif()
idf_component_register(
    SRCS "kvm_record.c" "record_audio.c" "record_audio_encoder.c" "dashcam.c" "frame_ring.c" "ts_mux.c" "keylog.c" "keymap_layouts.c" "ts_to_mp4.c"
    INCLUDE_DIRS "include"
    REQUIRES
        esp_common
    PRIV_REQUIRES
        kvm_audio
        ${audio_deps}
        video_pipeline
        kvm_hid
        kvm_config
        kvm_screentext
        kvm_atx
        kvm_notify
        kvm_storage
        esp_ringbuf
        esp_timer
        esp_psram
        fatfs
)
