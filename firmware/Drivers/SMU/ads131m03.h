/*
 * ads131m03.h
 *
 *  Created on: 27 Sept 2026
 *      Author: bodri
 */

#ifndef ADS131M03_H
#define ADS131M03_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ADS131M03_CHANNEL_COUNT 3u
#define ADS131M03_WORD_BYTES_24 3u
#define ADS131M03_FRAME_WORDS 5u
#define ADS131M03_FRAME_BYTES_24 (ADS131M03_FRAME_WORDS * ADS131M03_WORD_BYTES_24)

typedef struct {
    int32_t ch[ADS131M03_CHANNEL_COUNT];
    uint16_t status;
    uint16_t crc_received;
    uint16_t crc_calculated;
    uint32_t sequence;
    bool crc_ok;
} ads131m03_frame_t;

#define ADS_DMA_FRAME_BYTES 15u
#define ADS_DMA_RING_SIZE 64u

typedef struct {
    uint32_t status;
    int32_t ch0;
    int32_t ch1;
    int32_t ch2;
    uint16_t crc_rx;
    uint16_t crc_calc;
} ads131m03_dma_frame_t;

typedef struct {
    volatile uint32_t drdy_count;
    volatile uint32_t frame_count;

    volatile uint32_t dma_busy_count;
    volatile uint32_t spi_error_count;
    volatile uint32_t ring_overrun_count;
    volatile uint32_t crc_error_count;

    volatile uint32_t write_index;
    volatile uint32_t read_index;

    volatile bool dma_active;

    /* Latest decoded frame - useful in debugger */
    volatile ads131m03_dma_frame_t latest;

} ads131m03_dma_status_t;

void ads131m03_dma_init(void);

void ads131m03_dma_drdy_isr(void);
void ads131m03_dma_complete_isr(void);
void ads131m03_dma_error_isr(void);

bool ads131m03_dma_pop(ads131m03_dma_frame_t* frame);

const ads131m03_dma_status_t* ads131m03_dma_get_status(void);

/* Pure functions intentionally exposed for host tests. Default Rev-A format:
 * 24-bit words, output CRC enabled, CCITT polynomial, seed 0xFFFF. */
uint16_t ads131m03_crc16_ccitt(const uint8_t* data, size_t len);
bool ads131m03_decode_frame24(const uint8_t raw[ADS131M03_FRAME_BYTES_24], ads131m03_frame_t* out);

typedef struct {
    bool reset_ack_ok;
    bool id_ok;
    bool mode_ok;
    bool clock_ok;
    uint16_t reset_ack;
    uint16_t id;
    uint16_t mode;
    uint16_t clock;
    uint8_t raw[15];
} ads131m03_bringup_result_t;

/* Blocking, safe bench diagnostic. Run before normal ADS DMA acquisition. */
bool ads131m03_bringup_run(ads131m03_bringup_result_t* out);

#endif
