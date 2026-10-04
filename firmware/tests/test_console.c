#include "smu.h"
#include "smu_calibration.h"
#include "smu_console.h"
#include "smu_console_port.h"
#include "smu_log.h"
#include "smu_port.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static char input[1024], output[20000];
static size_t read_at, write_at;
static uint32_t now;
static bool lost, save_fail;
static unsigned saves, transport_inits;
static smu_context_t ctx;
static smu_cal_record_t active;

bool smu_console_port_init(void) {
    ++transport_inits;
    return true;
}

bool smu_console_port_read(uint8_t* byte) {
    if (!input[read_at])
        return false;
    *byte = (uint8_t)input[read_at++];
    return true;
}

bool smu_console_port_write(const char* text, size_t length) {
    assert(write_at + length < sizeof(output));
    memcpy(output + write_at, text, length);
    write_at += length;
    output[write_at] = 0;
    return true;
}

bool smu_console_port_rx_lost(void) {
    bool result = lost;
    lost = false;
    return result;
}

uint32_t smu_port_millis(void) {
    return now;
}

const smu_context_t* smu_get_context(void) {
    return &ctx;
}

void smu_get_measurement(smu_measurement_outputs_t* out) {
    smu_measurement_get_outputs(out);
}

smu_status_t smu_set_integration_us(uint32_t us) {
    return smu_measurement_set_precision_samples(us / 250u) ? SMU_OK : SMU_ERR_ARG;
}

smu_status_t smu_set_input_10m(bool value) {
    ctx.input_10m_requested = value;
    ctx.input_10m_active = value;
    return SMU_OK;
}

void smu_set_current_autorange(bool value) {
    ctx.current_autorange = value;
}

void smu_set_voltage_autorange(bool value) {
    ctx.voltage_autorange = value;
}

smu_status_t smu_set_current_range(smu_current_range_t range) {
    ctx.range = range;
    ctx.current_autorange = false;
    smu_measurement_set_current_range(range);
    return SMU_OK;
}

smu_status_t smu_set_voltage_range(smu_voltage_range_t range) {
    ctx.vrange = range;
    ctx.voltage_autorange = false;
    smu_measurement_set_voltage_range(range);
    return SMU_OK;
}

smu_cal_load_result_t smu_calibration_load_result(void) {
    return SMU_CAL_LOAD_OK;
}

const smu_cal_record_t* smu_calibration_get(void) {
    return &active;
}

bool smu_calibration_commit(const smu_cal_record_t* record) {
    if (save_fail)
        return false;
    active = *record;
    ++active.sequence;
    ++saves;
    return true;
}

static void send(const char* text) {
    snprintf(input, sizeof(input), "%s", text);
    read_at = write_at = 0;
    output[0] = 0;
    while (input[read_at])
        smu_console_process();
}

static void sample(int32_t code) {
    ads131m03_frame_t f = {.ch = {code, code, code}, .crc_ok = true};
    for (unsigned j = 0; j < SMU_PRECISION_GROUP_SAMPLES; ++j)
        smu_measurement_process_frame(&f);
}

static void point(const char* reference, int32_t code) {
    char text[100];
    snprintf(text, sizeof(text), "CAL:CAPTURE %s\r\n", reference);
    send(text);
    assert(strstr(output, "acquiring"));
    ads131m03_dma_frame_t frame = {.ch0 = code, .ch1 = code, .ch2 = code};
    for (unsigned i = 0; i < SMU_CAL_CAPTURE_SAMPLES; ++i)
        smu_console_frame(&frame, ctx.range, ctx.vrange);
    smu_console_process();
    assert(strstr(output, "OK POINT"));
}

int main(void) {
    ctx = (smu_context_t){.state = SMU_STATE_NORMAL, .range = SMU_RANGE_10MA, .vrange = SMU_VRANGE_6V, .measurement_valid = true, .current_autorange = true};
    smu_measurement_init(NULL, NULL);
    smu_measurement_set_voltage_range(ctx.vrange);
    smu_measurement_set_valid(true);
    sample(0);
    for (unsigned i = 0; i < 5; ++i)
        active.measurement.current[i].gain = 1;
    for (unsigned i = 0; i < 2; ++i)
        active.measurement.voltage[i].gain = 1;
    active.measurement.calbus.gain = 1;
    active.vforce.gain = 7;
    assert(smu_console_transport_init());
    assert(smu_log_write("ADC startup test\r\n"));
    assert(smu_console_init());
    assert(transport_inits == 1 && strstr(output, "ADC startup test"));
    assert(strstr(output, "serial ready"));
    send("ping\r\n");
    assert(strstr(output, "PONG"));
    ctx.acquisition_stale = true;
    ctx.measurement_age_ms = 25;
    ctx.adc_crc_errors = 3;
    send("ACQ?\n");
    assert(strstr(output, "stale=1 age_ms=25") && strstr(output, "crc=3"));
    send("STATUS?\n");
    assert(strstr(output, "fresh=0 settled=0 precision_ready=0 stale=1 age_ms=25"));
    assert(strstr(output, "WATCHDOG active=0 reset=0"));
    ctx.acquisition_stale = false;
    send("IMPEDANCE 10M\n");
    assert(ctx.input_10m_requested && ctx.input_10m_active);
    send("STATUS?\n");
    assert(strstr(output, "impedance_requested=10M impedance=10M"));
    send("IMPEDANCE HIGHZ\n");
    assert(!ctx.input_10m_requested && !ctx.input_10m_active);
    send("IMPEDANCE INVALID\n");
    assert(strstr(output, "ERR"));
    send("AUTORANGE:V ON\n");
    assert(ctx.voltage_autorange && ctx.current_autorange);
    send("STATUS?\n");
    assert(strstr(output, "autorange_v=1"));
    send("AUTORANGE:I OFF\n");
    assert(!ctx.current_autorange && ctx.voltage_autorange);
    send("CAL:BEGIN V\n");
    assert(strstr(output, "ERR"));
    send("RANGE:V 6V\n");
    assert(!ctx.voltage_autorange);
    send("AUTORANGE:I ON\n");
    assert(ctx.current_autorange);
    send("CAL:BEGIN V\n");
    assert(strstr(output, "ERR"));
    send("AUTORANGE OFF\n");
    assert(!ctx.current_autorange);
    send("CAL:BEGIN V\n");
    assert(strstr(output, "OK manual"));
    ctx.input_10m_active = ctx.input_10m_requested = true;
    send("CAL:CAPTURE 0\n");
    assert(strstr(output, "ERR"));
    ctx.input_10m_active = ctx.input_10m_requested = false;
    send("CAL:CAPTURE NAN\n");
    assert(strstr(output, "ERR"));
    send("CAL:CAPTURE 1E999\n");
    assert(strstr(output, "ERR"));
    send("CAL:FIT\n");
    assert(strstr(output, "ERR"));
    point("0.01", 0);
    float nominal = smu_voltage_from_adc(smu_ads_code_to_volts(1000000), ctx.vrange);
    char ref[40];
    snprintf(ref, sizeof(ref), "%.8f", nominal * 1.1f + 0.01f);
    point(ref, 1000000);
    send("CAL:FIT\n");
    assert(strstr(output, "OK FIT"));
    assert(saves == 0);
    save_fail = true;
    send("CAL:SAVE\n");
    assert(strstr(output, "failed"));
    assert(saves == 0);
    save_fail = false;
    send("CAL:SAVE\n");
    assert(saves == 1);
    assert(fabsf(active.measurement.voltage[SMU_VRANGE_6V].gain - 1.1f) < 1e-5f);
    assert(active.measurement.voltage[SMU_VRANGE_15V].gain == 1 && active.vforce.gain == 7);
    send("CAL:BEGIN I\n");
    send("CAL:CAPTURE 1E-5\n");
    assert(strstr(output, "acquiring"));
    send("RANGE:I 1MA\n");
    assert(strstr(output, "busy"));
    send("AUTORANGE:V ON\n");
    assert(strstr(output, "busy") && !ctx.voltage_autorange);
    send("IMPEDANCE 10M\n");
    assert(strstr(output, "busy") && !ctx.input_10m_active);
    smu_console_acquisition_gap();
    assert(strstr(output, "acquisition gap"));
    send("CAL:CAPTURE 1E-5\n");
    assert(strstr(output, "acquiring"));
    now += 2000;
    smu_console_process();
    assert(strstr(output, "timeout"));
    send("CAL:RESET\n");
    send("CAL:SAVE\n");
    assert(strstr(output, "ERR"));
    char long_line[200];
    memset(long_line, 'A', sizeof(long_line));
    long_line[198] = '\n';
    long_line[199] = 0;
    send(long_line);
    assert(strstr(output, "too long"));
    lost = true;
    send("PING\nPING\n");
    assert(strstr(output, "RX loss") && strstr(output, "PONG"));
    send("INTEGRATION 500US\n");
    assert(strstr(output, "OK integration"));
    send("MEAS?\n");
    assert(strstr(output, "integration_ms=0 integration_us=500"));
    send("INTEGRATION 2MS\n");
    assert(strstr(output, "OK integration"));
    send("INTEGRATION 20MS\n");
    assert(strstr(output, "OK integration"));
    assert(!strstr(output, "ERR"));
    for (unsigned i = 0; i < 80; ++i)
        sample(0);
    send("MEAS?\n");
    assert(strstr(output, "window=80/80") && strstr(output, "valid=1"));
    send("INTEGRATION INVALID\n");
    assert(strstr(output, "ERR"));
    send("CAL:BEGIN BUS\n");
    send("CAL:CAPTURE 0\n");
    ads131m03_dma_frame_t noisy = {0};
    for (unsigned i = 0; i < SMU_CAL_CAPTURE_SAMPLES; ++i) {
        noisy.ch2 = i % 2 ? 100000 : -100000;
        smu_console_frame(&noisy, ctx.range, ctx.vrange);
    }
    smu_console_process();
    assert(strstr(output, "unstable capture") && !strstr(output, "OK POINT"));
    send("CAL:POINTS?\n");
    assert(strstr(output, "points=0"));
    point("0", 0);
    point("0", 0);
    send("CAL:FIT\n");
    assert(strstr(output, "insufficient point span"));
    send("CAL:RESET\n");
    send("CAL:BEGIN I\n");
    point("0", 0);
    float current = smu_current_from_adc(smu_ads_code_to_volts(1000000), ctx.range);
    snprintf(ref, sizeof(ref), "%.9f", current * 1.2f);
    point(ref, 1000000);
    send("CAL:FIT\n");
    assert(strstr(output, "OK FIT"));
    send("CAL:BEGIN BUS\n");
    point("0", 0);
    snprintf(ref, sizeof(ref), "%.9f", smu_calbus_from_adc(smu_ads_code_to_volts(1000000)) * 0.9f);
    point(ref, 1000000);
    send("CAL:FIT\n");
    assert(strstr(output, "OK FIT"));
    send("CAL:SAVE\n");
    assert(saves == 2);
    assert(fabsf(active.measurement.current[SMU_RANGE_10MA - 1].gain - 1.2f) < 1e-5f);
    assert(fabsf(active.measurement.calbus.gain - 0.9f) < 1e-5f);
    assert(active.measurement.voltage[SMU_VRANGE_15V].gain == 1);
    send("CAL:BEGIN V\n");
    send("CAL:CAPTURE 1\n");
    ads131m03_dma_frame_t clipped = {.ch1 = 8388607};
    smu_console_frame(&clipped, ctx.range, ctx.vrange);
    assert(strstr(output, "clipping"));
    point("0", 0);
    point("1", 1000000);
    send("CAL:FIT\n");
    ++active.sequence;
    send("CAL:SAVE\n");
    assert(strstr(output, "active calibration changed") && saves == 2);
    send("CAL:RESET\n");
    send("CAL:BEGIN V\n");
    send("CAL:CAPTURE 1\n");
    ctx.vrange = SMU_VRANGE_15V;
    smu_console_frame(&clipped, ctx.range, ctx.vrange);
    assert(strstr(output, "range/state changed"));
    puts("serial console: all tests passed");
}

/* These suites exercise the instrument without starting physical IWDG. */
bool smu_watchdog_port_was_reset(void) {
    return false;
}

bool smu_watchdog_port_start(void) {
    return true;
}

bool smu_watchdog_port_refresh(void) {
    return true;
}
