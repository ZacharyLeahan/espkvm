// SPDX-License-Identifier: Apache-2.0
#include <stdint.h>
#include <stdatomic.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "sdkconfig.h"

/*
 * esp-hosted 3.0.9 calls the unaligned DMA allocator for each SDIO packet. Its
 * CONFIG_EH_HOST_PORT_DMA_PREFER_SPIRAM option only affects the aligned variant,
 * so concurrent video/audio can exhaust internal RAM while PSRAM is available.
 * P4 SDMMC supports PSRAM and performs cache maintenance, but requires aligned
 * addresses and whole cache lines. Byte-mode tails use the driver's existing
 * SDMMC_HOST_FLAG_ALLOC_ALIGNED_BUF bounce buffer. Keep the dependency untouched
 * and retain its original allocator when external RAM cannot satisfy a request.
 * Adapted from https://github.com/espkvm/espkvm/commit/c5761dc7db6e798e54bbcea1bbcde468f23c3319
 */
void *__real_eh_host_port_dma_alloc(size_t n);

void *__wrap_eh_host_port_dma_alloc(size_t n)
{
    const size_t line = CONFIG_CACHE_L2_CACHE_LINE_SIZE;
    if (n > SIZE_MAX - (line - 1)) {
        return NULL;
    }
    void *p = heap_caps_malloc((n + line - 1) & ~(line - 1),
                              MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA |
                              MALLOC_CAP_8BIT | MALLOC_CAP_CACHE_ALIGNED);
    static atomic_flag reported = ATOMIC_FLAG_INIT;
    if (p && !atomic_flag_test_and_set(&reported)) {
        ESP_LOGI("wifi_dma", "SDIO transfer buffers allocated in cache-aligned PSRAM");
    }
    return p ? p : __real_eh_host_port_dma_alloc(n);
}
