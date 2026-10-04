/*
 * ads131m03.c
 *
 *  Created on: 27 Sept 2026
 *      Author: bodri
 */

#include "ads131m03.h"
#include "smu_log.h"

#include "ads131m03_port.h"

#include <string.h>

static uint8_t tx_frame[ADS_DMA_FRAME_BYTES];
static uint8_t rx_frame[ADS_DMA_FRAME_BYTES];

static ads131m03_dma_frame_t ring[ADS_DMA_RING_SIZE];

static ads131m03_dma_status_t s;

/* ----------------------------------------------------------
 * Helpers
 * ---------------------------------------------------------- */

static uint32_t unpack_u24(const uint8_t* p) {
    return ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[2];
}

static int32_t unpack_s24(const uint8_t* p) {
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
uint16_t ads131m03_crc16_ccitt(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFFu;

    for (size_t i = 0; i < length; ++i) {
        crc ^= (uint16_t)data[i] << 8;

        for (uint32_t bit = 0; bit < 8u; ++bit) {
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

void ads131m03_dma_init(void) {
    memset(&s, 0, sizeof(s));
    memset(tx_frame, 0, sizeof(tx_frame));
    memset(rx_frame, 0, sizeof(rx_frame));
    memset(ring, 0, sizeof(ring));

    ads131m03_port_cs_deassert();
}

/* ----------------------------------------------------------
 * DRDY ISR
 * ---------------------------------------------------------- */

void ads131m03_dma_drdy_isr(void) {
    s.drdy_count++;

    /*
     * Never start another transaction while one is active.
     *
     * If this happens at 32 kSPS, SPI throughput or interrupt
     * latency is insufficient.
     */
    if (s.dma_active) {
        s.dma_busy_count++;
        return;
    }

    s.dma_active = true;

    ads131m03_port_cs_assert();

    if (!ads131m03_port_spi_dma_start(tx_frame, rx_frame, ADS_DMA_FRAME_BYTES)) {
        ads131m03_port_cs_deassert();

        s.dma_active = false;
        s.spi_error_count++;
    }
}

/* ----------------------------------------------------------
 * DMA complete ISR
 * ---------------------------------------------------------- */

void ads131m03_dma_complete_isr(void) {
    ads131m03_dma_frame_t f;

    ads131m03_port_cs_deassert();

    s.dma_active = false;

    f.status = unpack_u24(&rx_frame[0]);
    f.ch0 = unpack_s24(&rx_frame[3]);
    f.ch1 = unpack_s24(&rx_frame[6]);
    f.ch2 = unpack_s24(&rx_frame[9]);

    /*
     * CRC is MSB-aligned in its 24-bit communication word.
     */
    f.crc_rx = ((uint16_t)rx_frame[12] << 8) | (uint16_t)rx_frame[13];

    /*
     * STATUS + CH0 + CH1 + CH2 = 12 bytes.
     */
    f.crc_calc = ads131m03_crc16_ccitt(rx_frame, 12u);

    if (f.crc_calc != f.crc_rx) {
        s.crc_error_count++;

        /*
         * Don't put corrupted frames into the measurement
         * pipeline.
         */
        return;
    }

    s.latest = f;

    uint32_t next = (s.write_index + 1u) % ADS_DMA_RING_SIZE;

    if (next == s.read_index) {
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

void ads131m03_dma_error_isr(void) {
    ads131m03_port_cs_deassert();

    s.dma_active = false;
    s.spi_error_count++;
}

/* ----------------------------------------------------------
 * Foreground consumer
 * ---------------------------------------------------------- */

bool ads131m03_dma_pop(ads131m03_dma_frame_t* frame) {
    if (frame == NULL)
        return false;

    if (s.read_index == s.write_index)
        return false;

    *frame = ring[s.read_index];

    s.read_index = (s.read_index + 1u) % ADS_DMA_RING_SIZE;

    return true;
}

const ads131m03_dma_status_t* ads131m03_dma_get_status(void) {
    return &s;
}

bool ads131m03_decode_frame24(const uint8_t raw[ADS131M03_FRAME_BYTES_24], ads131m03_frame_t* out) {
    if (!raw || !out)
        return false;
    memset(out, 0, sizeof(*out));

    /* Commands/responses/register values contain 16 significant bits and are
     * MSB-aligned in a 24-bit word. */
    out->status = (uint16_t)(((uint16_t)raw[0] << 8) | raw[1]);
    out->ch[0] = unpack_s24(&raw[3]);
    out->ch[1] = unpack_s24(&raw[6]);
    out->ch[2] = unpack_s24(&raw[9]);
    out->crc_received = (uint16_t)(((uint16_t)raw[12] << 8) | raw[13]);

    /* Output CRC covers the preceding output words. In 24-bit word mode this
     * includes their padding bits; therefore calculate over the first 12 bytes. */
    out->crc_calculated = ads131m03_crc16_ccitt(raw, 12u);
    out->crc_ok = (out->crc_received == out->crc_calculated);
    return true;
}

/* Blocking device reset and register diagnostics; run before DMA acquisition. */
#define FRAME_BYTES 15u
#define CMD_RESET 0x0011u
#define CMD_RREG(a) ((uint16_t)(0xA000u | (((uint16_t)(a) & 0x3Fu) << 7)))
#define CMD_WREG(a) ((uint16_t)(0x6000u | (((uint16_t)(a) & 0x3Fu) << 7)))

#define CLOCK_CH_EN_MASK 0x0700u
#define CLOCK_OSR_MASK 0x001Cu
#define CLOCK_PWR_MASK 0x0003u

static void put_cmd(uint8_t f[FRAME_BYTES], uint16_t c) {
    memset(f, 0, FRAME_BYTES);
    f[0] = (uint8_t)(c >> 8);
    f[1] = (uint8_t)c;
    f[2] = 0;
}

static uint16_t word16(const uint8_t f[FRAME_BYTES]) {
    return (uint16_t)(((uint16_t)f[0] << 8) | f[1]);
}

static bool xfer_cmd_response(uint16_t cmd, uint16_t* response, uint8_t last[15]) {
    uint8_t tx[FRAME_BYTES], rx[FRAME_BYTES];
    put_cmd(tx, cmd);
    if (!ads131m03_port_transfer(tx, rx, FRAME_BYTES))
        return false;
    memset(tx, 0, sizeof(tx));
    memset(rx, 0, sizeof(rx));
    if (!ads131m03_port_transfer(tx, rx, FRAME_BYTES))
        return false;
    if (last)
        memcpy(last, rx, FRAME_BYTES);
    if (response)
        *response = word16(rx);
    return true;
}

static void logreg(const char* name, uint16_t v) {
    (void)smu_log_printf("ADC ADS %-5s = 0x%04X\r\n", name, (unsigned)v);
}

static bool write_clock(uint16_t clock) {
    uint8_t tx[FRAME_BYTES], rx[FRAME_BYTES];
    put_cmd(tx, CMD_WREG(0x03));
    tx[3] = (uint8_t)(clock >> 8);
    tx[4] = (uint8_t)clock;
    if (!ads131m03_port_transfer(tx, rx, FRAME_BYTES))
        return false;
    memset(tx, 0, sizeof(tx));
    if (!ads131m03_port_transfer(tx, rx, FRAME_BYTES))
        return false;
    return word16(rx) == (uint16_t)(0x4000u | (0x03u << 7));
}

bool ads131m03_bringup_run(ads131m03_bringup_result_t* o) {
    if (!o)
        return false;
    memset(o, 0, sizeof(*o));
    (void)smu_log_write("\r\nADC ADS131M03 bring-up\r\nOutput remains SAFE/OFF\r\n");
    if (!ads131m03_port_init())
        return false;

    /* SYNC/RESET low then high. Port must keep CS high during this operation. */
    ads131m03_port_reset(true);
    ads131m03_port_delay_ms(2);
    ads131m03_port_reset(false);
    if (!ads131m03_port_wait_ready(100)) {
        (void)smu_log_write("ADC ADS DRDY ready timeout\r\n");
        return false;
    }

    /* RESET command is latched only after the entire frame. FF23 appears in the next frame. */
    if (!xfer_cmd_response(CMD_RESET, &o->reset_ack, o->raw))
        return false;
    o->reset_ack_ok = (o->reset_ack == 0xFF23u);
    logreg("RESET", o->reset_ack);
    if (!o->reset_ack_ok)
        return false;

    /* RESET command restarts register acquisition; wait for interface-ready again. */
    if (!ads131m03_port_wait_ready(100))
        return false;

    /* Single-register RREG returns register data as first word of following frame. */
    if (!xfer_cmd_response(CMD_RREG(0x00), &o->id, o->raw))
        return false;
    if (!xfer_cmd_response(CMD_RREG(0x02), &o->mode, o->raw))
        return false;
    /* Explicitly select OSR while preserving the reset channel enables, TBM=0
     * and HR power mode. Full 24-bit frames, input CRC disabled by reset MODE. */
    const uint16_t desired_clock = 0x0702u | ADS131M03_CLOCK_OSR_BITS;
    if (!write_clock(desired_clock))
        return false;
    /* Let the digital filter settle before the readback and acquisition. */
    ads131m03_port_delay_ms(2);
    if (!xfer_cmd_response(CMD_RREG(0x03), &o->clock, o->raw))
        return false;
    o->id_ok = ((o->id & 0xFF00u) == 0x2300u); /* low ID byte is reserved/variable */
    o->mode_ok = (o->mode == 0x0510u);
    /* CLOCK expected configuration:
     * CH2_EN, CH1_EN, CH0_EN = 1
     * TBM = 0
     * OSR from the shared rate configuration (128 at 32 kSPS)
     * PWR = 10
     */
    o->clock_ok = (o->clock == desired_clock);
    logreg("ID", o->id);
    logreg("MODE", o->mode);
    logreg("CLOCK", o->clock);
    (void)smu_log_printf("ADC configured rate=%lu Hz (8.192 MHz CLKIN required)\r\n", (unsigned long)ADS131M03_SAMPLE_RATE_HZ);
    (void)smu_log_write((o->id_ok && o->mode_ok && o->clock_ok) ? "ADC ADS131M03 COMMUNICATION: PASS\r\n" : "ADC ADS131M03 REGISTER CHECK: FAIL\r\n");
    return o->id_ok && o->mode_ok && o->clock_ok;
}
