#include "ads131m03_bringup.h"
#include "ads131m03_bringup_port.h"
#include <string.h>
#include <stdio.h>

#define FRAME_BYTES 15u
#define CMD_RESET   0x0011u
#define CMD_RREG(a) ((uint16_t)(0xA000u | (((uint16_t)(a) & 0x3Fu) << 7)))

#define CLOCK_CH_EN_MASK   0x0700u
#define CLOCK_OSR_MASK     0x001Cu
#define CLOCK_PWR_MASK     0x0003u

static void put_cmd(uint8_t f[FRAME_BYTES], uint16_t c) {
    memset(f,0,FRAME_BYTES); f[0]=(uint8_t)(c>>8); f[1]=(uint8_t)c; f[2]=0;
}
static uint16_t word16(const uint8_t f[FRAME_BYTES]) {
    return (uint16_t)(((uint16_t)f[0]<<8)|f[1]);
}
static bool xfer_cmd_response(uint16_t cmd, uint16_t *response, uint8_t last[15]) {
    uint8_t tx[FRAME_BYTES],rx[FRAME_BYTES];
    put_cmd(tx,cmd);
    if(!ads131m03_bu_port_transfer(tx,rx,FRAME_BYTES)) return false;
    memset(tx,0,sizeof(tx)); memset(rx,0,sizeof(rx));
    if(!ads131m03_bu_port_transfer(tx,rx,FRAME_BYTES)) return false;
    if(last) memcpy(last,rx,FRAME_BYTES);
    if(response) *response=word16(rx);
    return true;
}
static void logreg(const char *name,uint16_t v) {
    char b[48]; (void)snprintf(b,sizeof(b),"ADS %-5s = 0x%04X\r\n",name,(unsigned)v);
    ads131m03_bu_port_log(b);
}

bool ads131m03_bringup_run(ads131m03_bringup_result_t *o) {
    if(!o) return false;
    memset(o,0,sizeof(*o));
    ads131m03_bu_port_log("\r\nSMU ADS131M03 bring-up\r\nOutput remains SAFE/OFF\r\n");
    if(!ads131m03_bu_port_init()) return false;

    /* SYNC/RESET low then high. Port must keep CS high during this operation. */
    ads131m03_bu_port_reset(true); ads131m03_bu_port_delay_ms(2);
    ads131m03_bu_port_reset(false);
    if(!ads131m03_bu_port_wait_ready(100)) {
        ads131m03_bu_port_log("ADS DRDY ready timeout\r\n"); return false;
    }

    /* RESET command is latched only after the entire frame. FF23 appears in the next frame. */
    if(!xfer_cmd_response(CMD_RESET,&o->reset_ack,o->raw)) return false;
    o->reset_ack_ok=(o->reset_ack==0xFF23u); logreg("RESET",o->reset_ack);
    if(!o->reset_ack_ok) return false;

    /* RESET command restarts register acquisition; wait for interface-ready again. */
    if(!ads131m03_bu_port_wait_ready(100)) return false;

    /* Single-register RREG returns register data as first word of following frame. */
    if(!xfer_cmd_response(CMD_RREG(0x00),&o->id,o->raw)) return false;
    if(!xfer_cmd_response(CMD_RREG(0x02),&o->mode,o->raw)) return false;
    if(!xfer_cmd_response(CMD_RREG(0x03),&o->clock,o->raw)) return false;
    o->id_ok=((o->id & 0xFF00u)==0x2300u); /* low ID byte is reserved/variable */
    o->mode_ok=(o->mode==0x0510u);
    /* CLOCK expected configuration:
     * CH2_EN, CH1_EN, CH0_EN = 1
     * TBM = 0
     * OSR = 011 (1024)
     * PWR = 10
     */
    o->clock_ok =
        ((o->clock & CLOCK_CH_EN_MASK) == 0x0700u) &&
        ((o->clock & CLOCK_OSR_MASK)   == 0x000Cu) &&
        ((o->clock & CLOCK_PWR_MASK)   == 0x0002u);
    logreg("ID",o->id); logreg("MODE",o->mode); logreg("CLOCK",o->clock);
    ads131m03_bu_port_log((o->id_ok&&o->mode_ok&&o->clock_ok)?
        "ADS131M03 COMMUNICATION: PASS\r\n":"ADS131M03 REGISTER CHECK: FAIL\r\n");
    return o->id_ok&&o->mode_ok&&o->clock_ok;
}
