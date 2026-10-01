#ifndef AD5686_H
#define AD5686_H
#include <stdbool.h>
#include <stdint.h>
typedef enum { AD5686_CH_A = 0, AD5686_CH_B, AD5686_CH_C, AD5686_CH_D } ad5686_channel_t;
bool ad5686_init(void);
bool ad5686_write_input(ad5686_channel_t ch, uint16_t code);
bool ad5686_write_and_update(ad5686_channel_t ch, uint16_t code);
void ad5686_ldac_pulse(void);
void ad5686_reset_pulse(void);
#endif
