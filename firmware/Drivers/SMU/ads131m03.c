#include "ads131m03.h"
#include "ads131m03_port.h"
#include <string.h>

/* 24-bit mode: DIN = NULL command + four zero words. Input CRC remains disabled
 * in Phase 1. DOUT = response/status + CH0 + CH1 + CH2 + output CRC. */
static uint8_t s_tx[ADS131M03_FRAME_BYTES_24];
static uint8_t s_rx[ADS131M03_FRAME_BYTES_24];
static volatile bool s_running;
static volatile bool s_dma_active;
static ads131m03_frame_t s_ring[ADS131M03_RING_CAPACITY];
static volatile uint16_t s_head;
static volatile uint16_t s_tail;
static uint32_t s_sequence;
static ads131m03_stats_t s_stats;

static int32_t sign_extend24(const uint8_t *p)
{
    uint32_t u = ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
    if (u & 0x00800000u) u |= 0xFF000000u;
    return (int32_t)u;
}

uint16_t ads131m03_crc16_ccitt(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u)
                                  : (uint16_t)(crc << 1);
    }
    return crc;
}

bool ads131m03_decode_frame24(const uint8_t raw[ADS131M03_FRAME_BYTES_24],
                              ads131m03_frame_t *out)
{
    if (!raw || !out) return false;
    memset(out, 0, sizeof(*out));

    /* Commands/responses/register values contain 16 significant bits and are
     * MSB-aligned in a 24-bit word. */
    out->status = (uint16_t)(((uint16_t)raw[0] << 8) | raw[1]);
    out->ch[0] = sign_extend24(&raw[3]);
    out->ch[1] = sign_extend24(&raw[6]);
    out->ch[2] = sign_extend24(&raw[9]);
    out->crc_received = (uint16_t)(((uint16_t)raw[12] << 8) | raw[13]);

    /* Output CRC covers the preceding output words. In 24-bit word mode this
     * includes their padding bits; therefore calculate over the first 12 bytes. */
    out->crc_calculated = ads131m03_crc16_ccitt(raw, 12u);
    out->crc_ok = (out->crc_received == out->crc_calculated);
    return true;
}

bool ads131m03_init(void)
{
    memset(s_tx, 0, sizeof(s_tx));
    memset(s_rx, 0, sizeof(s_rx));
    memset(&s_stats, 0, sizeof(s_stats));
    s_head = s_tail = 0u;
    s_sequence = 0u;
    s_running = false;
    s_dma_active = false;
    return ads131m03_port_init();
}

bool ads131m03_start(void) { s_running = true; return true; }
void ads131m03_stop(void) { s_running = false; }

void ads131m03_drdy_isr(void)
{
    if (!s_running) return;
    if (s_dma_active || ads131m03_port_spi_dma_busy()) {
        s_stats.dma_busy_drops++;
        return;
    }
    ads131m03_port_cs_assert();
    s_dma_active = true;
    if (!ads131m03_port_spi_dma_start(s_tx, s_rx, sizeof(s_rx))) {
        s_dma_active = false;
        ads131m03_port_cs_deassert();
        s_stats.dma_busy_drops++;
    }
}

void ads131m03_spi_dma_complete_isr(void)
{
    ads131m03_port_cs_deassert();
    s_dma_active = false;
    s_stats.frames_received++;

    ads131m03_frame_t f;
    if (!ads131m03_decode_frame24(s_rx, &f)) return;
    f.sequence = s_sequence++;
    if (!f.crc_ok) {
        s_stats.crc_errors++;
        return; /* Never admit corrupt conversion data to the measurement path. */
    }

    uint16_t next = (uint16_t)((s_head + 1u) % ADS131M03_RING_CAPACITY);
    if (next == s_tail) {
        s_stats.ring_overruns++;
        return; /* Preserve queued samples; drop newest on overrun. */
    }
    s_ring[s_head] = f;
    s_head = next;
    s_stats.frames_pushed++;
}

bool ads131m03_pop_frame(ads131m03_frame_t *out)
{
    if (!out || s_tail == s_head) return false;
    *out = s_ring[s_tail];
    s_tail = (uint16_t)((s_tail + 1u) % ADS131M03_RING_CAPACITY);
    return true;
}

void ads131m03_get_stats(ads131m03_stats_t *out)
{
    if (out) *out = s_stats;
}
