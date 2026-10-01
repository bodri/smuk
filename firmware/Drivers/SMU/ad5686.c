#include "ad5686.h"

#include "ad5686_port.h"

/* 24-bit input shift register: command[23:20], address[19:16], data[15:0]. */
#define CMD_WRITE_INPUT 0x1u
#define CMD_WRITE_AND_UPDATE 0x3u

static bool write_cmd(uint8_t cmd, ad5686_channel_t ch, uint16_t code) {
    if (ch > AD5686_CH_D)
        return false;
    const uint8_t addr = (uint8_t)(1u << (unsigned)ch);
    const uint8_t tx[3] = {(uint8_t)((cmd << 4) | addr), (uint8_t)(code >> 8), (uint8_t)code};
    return ad5686_port_write(tx, sizeof(tx));
}

bool ad5686_init(void) {
    return ad5686_port_init();
}

bool ad5686_write_input(ad5686_channel_t ch, uint16_t code) {
    return write_cmd(CMD_WRITE_INPUT, ch, code);
}

bool ad5686_write_and_update(ad5686_channel_t ch, uint16_t code) {
    return write_cmd(CMD_WRITE_AND_UPDATE, ch, code);
}

void ad5686_ldac_pulse(void) {
    ad5686_port_ldac_pulse();
}

void ad5686_reset_pulse(void) {
    ad5686_port_reset_pulse();
}
