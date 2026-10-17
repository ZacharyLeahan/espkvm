// SPDX-License-Identifier: Apache-2.0
#include "record_audio_encoder.h"
#include "sdkconfig.h"

#if CONFIG_KVM_ENABLE_AUDIO
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "esp_aac_enc.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

struct record_audio_encoder {
    void *codec;
    uint8_t *input;
    uint8_t *output;
    size_t input_bytes;
    size_t output_bytes;
    unsigned channels;
    unsigned rate_index;
};

static int aac_rate_index(uint32_t rate)
{
    switch (rate) {
    case 96000: return 0;
    case 88200: return 1;
    case 48000: return 3;
    case 44100: return 4;
    case 32000: return 5;
    default: return -1;
    }
}

static esp_err_t codec_result(esp_audio_err_t result)
{
    switch (result) {
    case ESP_AUDIO_ERR_OK: return ESP_OK;
    case ESP_AUDIO_ERR_MEM_LACK: return ESP_ERR_NO_MEM;
    case ESP_AUDIO_ERR_INVALID_PARAMETER: return ESP_ERR_INVALID_ARG;
    case ESP_AUDIO_ERR_NOT_SUPPORT: return ESP_ERR_NOT_SUPPORTED;
    case ESP_AUDIO_ERR_DATA_LACK:
    case ESP_AUDIO_ERR_BUFF_NOT_ENOUGH: return ESP_ERR_INVALID_SIZE;
    default: return ESP_FAIL;
    }
}

static void *working_buffer(size_t bytes, size_t alignment)
{
    // At least 16-byte alignment also suits the P4 assembly implementation.
    if (alignment < 16) alignment = 16;
    if (alignment > 64 || (alignment & (alignment - 1))) return NULL;
    return heap_caps_aligned_alloc(alignment, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

esp_err_t record_audio_encoder_open(uint32_t rate, unsigned channels,
                                    record_audio_encoder_t **out)
{
    if (!out) return ESP_ERR_INVALID_ARG;
    *out = NULL;
    int index = aac_rate_index(rate);
    if (index < 0 || (channels != 1 && channels != 2)) return ESP_ERR_INVALID_ARG;

    esp_aac_enc_config_t config = ESP_AAC_ENC_CONFIG_DEFAULT();
    config.sample_rate = rate;
    config.channel = channels;
    config.bits_per_sample = 16;
    // This satisfies the codec's minimum at every supported rate, including 96 kHz.
    config.bitrate = 80000 * channels;
    config.adts_used = true;
    esp_audio_enc_frame_info_t info = {0};
    esp_err_t err = codec_result(esp_aac_enc_get_frame_info_by_cfg(&config, &info));
    if (err != ESP_OK) return err;
    const size_t input_bytes = RECORD_AUDIO_FRAME_SAMPLES * channels * sizeof(int16_t);
    // ADTS has a 13-bit frame length. Keep allocation bounded if a dependency changes.
    if (info.in_frame_size != (int)input_bytes || info.out_frame_size < 7 ||
        info.out_frame_size > 8191) return ESP_ERR_NOT_SUPPORTED;

    record_audio_encoder_t *encoder = calloc(1, sizeof(*encoder));
    if (!encoder) return ESP_ERR_NO_MEM;
    encoder->input_bytes = input_bytes;
    encoder->output_bytes = info.out_frame_size;
    encoder->channels = channels;
    encoder->rate_index = index;
    encoder->input = working_buffer(input_bytes, info.in_frame_align);
    encoder->output = working_buffer(encoder->output_bytes, info.out_frame_align);
    if (!encoder->input || !encoder->output) {
        record_audio_encoder_close(encoder);
        return ESP_ERR_NO_MEM;
    }
    // Direct AAC calls avoid the common registry and unrelated codec instances.
    err = codec_result(esp_aac_enc_open(&config, sizeof(config), &encoder->codec));
    if (err != ESP_OK) {
        record_audio_encoder_close(encoder);
        return err;
    }
    ESP_LOGI("record_aac", "AAC-LC %lu Hz, %u ch, %u bit/s; PCM=%u ADTS capacity=%u",
             (unsigned long)rate, channels, 80000 * channels,
             (unsigned)encoder->input_bytes, (unsigned)encoder->output_bytes);
    *out = encoder;
    return ESP_OK;
}

static bool one_adts_frame(const record_audio_encoder_t *encoder, size_t length)
{
    const uint8_t *p = encoder->output;
    if (length < 7 || length > encoder->output_bytes || p[0] != 0xff ||
        (p[1] & 0xf6) != 0xf0 || (p[2] >> 6) != 1 ||
        ((p[2] >> 2) & 15) != encoder->rate_index ||
        (((p[2] & 1) << 2) | (p[3] >> 6)) != encoder->channels || (p[6] & 3)) return false;
    const size_t header_bytes = (p[1] & 1) ? 7 : 9;
    const size_t frame_bytes = ((size_t)(p[3] & 3) << 11) | ((size_t)p[4] << 3) | (p[5] >> 5);
    return length > header_bytes && frame_bytes == length;
}

esp_err_t record_audio_encoder_encode(record_audio_encoder_t *encoder,
                                      const int16_t *pcm, size_t frames,
                                      const uint8_t **adts, size_t *length)
{
    if (adts) *adts = NULL;
    if (length) *length = 0;
    if (!encoder || !pcm || !adts || !length) return ESP_ERR_INVALID_ARG;
    if (frames != RECORD_AUDIO_FRAME_SAMPLES) return ESP_ERR_INVALID_SIZE;
    memcpy(encoder->input, pcm, encoder->input_bytes);
    esp_audio_enc_in_frame_t input = {.buffer = encoder->input, .len = encoder->input_bytes};
    esp_audio_enc_out_frame_t output = {.buffer = encoder->output, .len = encoder->output_bytes};
    esp_err_t err = codec_result(esp_aac_enc_process(encoder->codec, &input, &output));
    if (err != ESP_OK) return err;
    // The muxer gets one access unit, never a truncated or silently changed stream.
    if (!one_adts_frame(encoder, output.encoded_bytes)) return ESP_ERR_INVALID_RESPONSE;
    *adts = encoder->output;
    *length = output.encoded_bytes;
    return ESP_OK;
}

esp_err_t record_audio_encoder_reset(record_audio_encoder_t *encoder)
{
    if (!encoder) return ESP_ERR_INVALID_ARG;
    return codec_result(esp_aac_enc_reset(encoder->codec));
}

void record_audio_encoder_close(record_audio_encoder_t *encoder)
{
    if (!encoder) return;
    if (encoder->codec) esp_aac_enc_close(encoder->codec);
    heap_caps_free(encoder->input);
    heap_caps_free(encoder->output);
    free(encoder);
}
#else
esp_err_t record_audio_encoder_open(uint32_t rate, unsigned channels,
                                    record_audio_encoder_t **out)
{
    (void)rate; (void)channels;
    if (!out) return ESP_ERR_INVALID_ARG;
    *out = NULL;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t record_audio_encoder_encode(record_audio_encoder_t *encoder,
                                      const int16_t *pcm, size_t frames,
                                      const uint8_t **adts, size_t *length)
{
    (void)encoder; (void)pcm; (void)frames;
    if (adts) *adts = NULL;
    if (length) *length = 0;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t record_audio_encoder_reset(record_audio_encoder_t *encoder)
{
    (void)encoder;
    return ESP_ERR_NOT_SUPPORTED;
}

void record_audio_encoder_close(record_audio_encoder_t *encoder)
{
    (void)encoder;
}
#endif
