#ifndef ADS131M03_BRINGUP_H
#define ADS131M03_BRINGUP_H
#include <stdbool.h>
#include <stdint.h>

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
bool ads131m03_bringup_run(ads131m03_bringup_result_t *out);
#endif
