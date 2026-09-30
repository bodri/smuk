#ifndef ADS131M03_H
#define ADS131M03_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define ADS131M03_CHANNEL_COUNT 3u
#define ADS131M03_WORD_BYTES_24 3u
#define ADS131M03_FRAME_WORDS 5u
#define ADS131M03_FRAME_BYTES_24 (ADS131M03_FRAME_WORDS * ADS131M03_WORD_BYTES_24)
#define ADS131M03_RING_CAPACITY 256u

typedef struct {
    int32_t ch[ADS131M03_CHANNEL_COUNT];
    uint16_t status;
    uint16_t crc_received;
    uint16_t crc_calculated;
    uint32_t sequence;
    bool crc_ok;
} ads131m03_frame_t;

typedef struct {
    uint32_t frames_received;
    uint32_t frames_pushed;
    uint32_t crc_errors;
    uint32_t ring_overruns;
    uint32_t dma_busy_drops;
} ads131m03_stats_t;

/* Device-facing API. */
bool ads131m03_init(void);
bool ads131m03_start(void);
void ads131m03_stop(void);
void ads131m03_drdy_isr(void);              /* Starts SPI DMA only. */
void ads131m03_spi_dma_complete_isr(void);  /* Decodes/validates/pushes only. */
bool ads131m03_pop_frame(ads131m03_frame_t *out);
void ads131m03_get_stats(ads131m03_stats_t *out);

/* Pure functions intentionally exposed for host tests. Default Rev-A format:
 * 24-bit words, output CRC enabled, CCITT polynomial, seed 0xFFFF. */
uint16_t ads131m03_crc16_ccitt(const uint8_t *data, size_t len);
bool ads131m03_decode_frame24(const uint8_t raw[ADS131M03_FRAME_BYTES_24],
                              ads131m03_frame_t *out);

#endif
