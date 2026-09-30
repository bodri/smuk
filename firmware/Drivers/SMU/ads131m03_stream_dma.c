/*
 * ads131m03_stream_dma.c
 *
 *  Created on: 27 Sept 2026
 *      Author: bodri
 */

#include "ads131m03_stream_dma.h"

#include "main.h"
#include "spi.h"

#include <string.h>

extern SPI_HandleTypeDef hspi1;

#define ADS_CS_PORT GPIOB
#define ADS_CS_PIN  GPIO_PIN_8

static uint8_t tx_frame[ADS_DMA_FRAME_BYTES];
static uint8_t rx_frame[ADS_DMA_FRAME_BYTES];

static ads131m03_dma_frame_t ring[ADS_DMA_RING_SIZE];

static ads131m03_dma_status_t s;


/* ----------------------------------------------------------
 * Helpers
 * ---------------------------------------------------------- */

static uint32_t unpack_u24(const uint8_t *p)
{
    return ((uint32_t)p[0] << 16) |
           ((uint32_t)p[1] << 8)  |
            (uint32_t)p[2];
}


static int32_t unpack_s24(const uint8_t *p)
{
    uint32_t x = unpack_u24(p);

    if ((x & 0x00800000u) != 0u)
        x |= 0xFF000000u;

    return (int32_t)x;
}


/*
 * ADS131M03 default output CRC:
 *
 * CRC-16/CCITT
 * polynomial = 0x1021
 * seed       = 0xFFFF
 *
 * For 24-bit communication the CRC itself occupies a
 * 24-bit SPI word:
 *
 *      CRC[15:8] CRC[7:0] 0x00
 *
 * CRC is calculated over the preceding output words.
 */
static uint16_t crc16_ccitt(const uint8_t *data,
                            uint32_t length)
{
    uint16_t crc = 0xFFFFu;

    for (uint32_t i = 0; i < length; ++i)
    {
        crc ^= (uint16_t)data[i] << 8;

        for (uint32_t bit = 0; bit < 8u; ++bit)
        {
            if ((crc & 0x8000u) != 0u)
                crc = (uint16_t)((crc << 1) ^ 0x1021u);
            else
                crc <<= 1;
        }
    }

    return crc;
}


/* ----------------------------------------------------------
 * Public initialization
 * ---------------------------------------------------------- */

void ads131m03_dma_init(void)
{
    memset(&s, 0, sizeof(s));
    memset(tx_frame, 0, sizeof(tx_frame));
    memset(rx_frame, 0, sizeof(rx_frame));
    memset(ring, 0, sizeof(ring));

    HAL_GPIO_WritePin(ADS_CS_PORT,
                      ADS_CS_PIN,
                      GPIO_PIN_SET);
}


/* ----------------------------------------------------------
 * DRDY ISR
 * ---------------------------------------------------------- */

void ads131m03_dma_drdy_isr(void)
{
    s.drdy_count++;

    /*
     * Never start another transaction while one is active.
     *
     * If this happens at 32 kSPS, SPI throughput or interrupt
     * latency is insufficient.
     */
    if (s.dma_active)
    {
        s.dma_busy_count++;
        return;
    }

    s.dma_active = true;

    HAL_GPIO_WritePin(ADS_CS_PORT,
                      ADS_CS_PIN,
                      GPIO_PIN_RESET);

    HAL_StatusTypeDef rc =
        HAL_SPI_TransmitReceive_DMA(&hspi1,
                                    tx_frame,
                                    rx_frame,
                                    ADS_DMA_FRAME_BYTES);

    if (rc != HAL_OK)
    {
        HAL_GPIO_WritePin(ADS_CS_PORT,
                          ADS_CS_PIN,
                          GPIO_PIN_SET);

        s.dma_active = false;
        s.spi_error_count++;
    }
}


/* ----------------------------------------------------------
 * DMA complete ISR
 * ---------------------------------------------------------- */

void ads131m03_dma_complete_isr(void)
{
    ads131m03_dma_frame_t f;

    HAL_GPIO_WritePin(ADS_CS_PORT,
                      ADS_CS_PIN,
                      GPIO_PIN_SET);

    s.dma_active = false;

    f.status = unpack_u24(&rx_frame[0]);
    f.ch0    = unpack_s24(&rx_frame[3]);
    f.ch1    = unpack_s24(&rx_frame[6]);
    f.ch2    = unpack_s24(&rx_frame[9]);

    /*
     * CRC is MSB-aligned in its 24-bit communication word.
     */
    f.crc_rx =
        ((uint16_t)rx_frame[12] << 8) |
         (uint16_t)rx_frame[13];

    /*
     * STATUS + CH0 + CH1 + CH2 = 12 bytes.
     */
    f.crc_calc = crc16_ccitt(rx_frame, 12u);

    if (f.crc_calc != f.crc_rx)
    {
        s.crc_error_count++;

        /*
         * Don't put corrupted frames into the measurement
         * pipeline.
         */
        return;
    }

    s.latest = f;

    uint32_t next =
        (s.write_index + 1u) % ADS_DMA_RING_SIZE;

    if (next == s.read_index)
    {
        /*
         * Consumer isn't keeping up.
         *
         * Drop newest frame rather than overwrite unread data.
         */
        s.ring_overrun_count++;
        return;
    }

    ring[s.write_index] = f;
    s.write_index = next;

    s.frame_count++;
}


void ads131m03_dma_error_isr(void)
{
    HAL_GPIO_WritePin(ADS_CS_PORT,
                      ADS_CS_PIN,
                      GPIO_PIN_SET);

    s.dma_active = false;
    s.spi_error_count++;
}


/* ----------------------------------------------------------
 * Foreground consumer
 * ---------------------------------------------------------- */

bool ads131m03_dma_pop(ads131m03_dma_frame_t *frame)
{
    if (frame == NULL)
        return false;

    if (s.read_index == s.write_index)
        return false;

    *frame = ring[s.read_index];

    s.read_index =
        (s.read_index + 1u) % ADS_DMA_RING_SIZE;

    return true;
}


const ads131m03_dma_status_t *
ads131m03_dma_get_status(void)
{
    return &s;
}
