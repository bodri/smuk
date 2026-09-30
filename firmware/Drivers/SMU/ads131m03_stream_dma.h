/*
 * ads131m03_stream_dma.h
 *
 *  Created on: 27 Sept 2026
 *      Author: bodri
 */

#ifndef ADS131M03_STREAM_DMA_H
#define ADS131M03_STREAM_DMA_H

#include <stdint.h>
#include <stdbool.h>

#define ADS_DMA_FRAME_BYTES     15u
#define ADS_DMA_RING_SIZE       64u

typedef struct
{
    uint32_t status;
    int32_t  ch0;
    int32_t  ch1;
    int32_t  ch2;
    uint16_t crc_rx;
    uint16_t crc_calc;
} ads131m03_dma_frame_t;

typedef struct
{
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

bool ads131m03_dma_pop(ads131m03_dma_frame_t *frame);

const ads131m03_dma_status_t *
ads131m03_dma_get_status(void);

#endif
