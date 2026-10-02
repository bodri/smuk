#include "smu_console.h"
#include "smu.h"
#include "smu_calibration.h"
#include "smu_console_port.h"
#include "smu_port.h"
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#define LINE_SIZE 128u
#define POINTS 8u
#define CAPTURE_SAMPLES 256u
#define CAPTURE_TIMEOUT_MS 2000u
static char line[LINE_SIZE];
static size_t used;
static bool discard_line, output_lost;
static smu_cal_record_t staged;
static bool dirty;

static enum { TARGET_NONE, TARGET_V, TARGET_I, TARGET_BUS } target;

static smu_current_range_t current_range;
static smu_voltage_range_t voltage_range;
static bool capture_input_10m;
static float x[POINTS], y[POINTS];
static unsigned points, acquired;
static int64_t code_sum;
static bool capturing;
static uint32_t started;
static float reference;
static bool ready;
static bool echo;

static void reply(const char* format, ...) {
    char text[384];
    va_list args;
    va_start(args, format);
    int n = vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    if (n < 0 || (size_t)n >= sizeof(text) || !smu_console_port_write(text, (size_t)n))
        output_lost = true;
}

/* Fixed decimal formatting avoids linking newlib's large printf float backend. */
static const char* number(float value, char text[32]) {
    if (!isfinite(value) || fabsf(value) > 1000000.0f) {
        strcpy(text, "INVALID");
        return text;
    }
    int64_t scaled = (int64_t)llroundf(value * 1000000000.0f);
    bool negative = scaled < 0;
    uint64_t magnitude = (uint64_t)(negative ? -scaled : scaled);
    (void)snprintf(text, 32, "%s%lu.%09lu", negative ? "-" : "", (unsigned long)(magnitude / 1000000000u), (unsigned long)(magnitude % 1000000000u));
    return text;
}

static const char* irange_name(smu_current_range_t r) {
    static const char* names[] = {"NONE", "1.5A", "100MA", "10MA", "1MA", "100UA"};
    return r >= SMU_RANGE_NONE && r <= SMU_RANGE_100UA ? names[r] : "INVALID";
}

static const char* vrange_name(smu_voltage_range_t r) {
    return r == SMU_VRANGE_6V ? "6V" : "15V";
}

static bool stable(void) {
    const smu_context_t* ctx = smu_get_context();
    smu_measurement_outputs_t out;
    smu_get_measurement(&out);
    return ctx->state == SMU_STATE_NORMAL && ctx->measurement_valid && !ctx->current_autorange && !ctx->voltage_autorange && out.fast.valid && !out.fast.overload && !out.fast.range_transition &&
           out.fast.range == ctx->range && out.fast.vrange == ctx->vrange && ctx->input_10m_requested == ctx->input_10m_active;
}

static bool same_range(void) {
    const smu_context_t* ctx = smu_get_context();
    return ctx->range == current_range && ctx->vrange == voltage_range && ctx->input_10m_requested == capture_input_10m && ctx->input_10m_active == capture_input_10m;
}

static void show_cal(void) {
    char a[32], b[32];
    const smu_cal_record_t* active = smu_calibration_get();
    reply("CAL active sequence=%lu staged_dirty=%u\r\n", (unsigned long)active->sequence, dirty);
    for (unsigned i = 0; i < 5; ++i)
        reply("I %s gain=%s offset_A=%s\r\n", irange_name((smu_current_range_t)(i + 1)), number(staged.measurement.current[i].gain, a), number(staged.measurement.current[i].offset, b));
    for (unsigned i = 0; i < 2; ++i)
        reply("V %s gain=%s offset_V=%s\r\n", vrange_name((smu_voltage_range_t)i), number(staged.measurement.voltage[i].gain, a), number(staged.measurement.voltage[i].offset, b));
    reply("BUS gain=%s offset_V=%s\r\n", number(staged.measurement.calbus.gain, a), number(staged.measurement.calbus.offset, b));
}

static void fit(void) {
    if (target == TARGET_NONE || capturing || points < 2 || !stable() || !same_range()) {
        reply("ERR fit requires >=2 points and unchanged fixed ranges\r\n");
        return;
    }
    smu_linear_cal_t coefficient;
    if (!smu_calibration_fit_linear(x, y, points, &coefficient)) {
        reply("ERR invalid or degenerate fit\r\n");
        return;
    }
    smu_linear_cal_t* destination = target == TARGET_V ? &staged.measurement.voltage[voltage_range] : target == TARGET_I ? &staged.measurement.current[current_range - 1] : &staged.measurement.calbus;
    *destination = coefficient;
    dirty = true;
    char a[32], b[32];
    reply("OK FIT gain=%s offset=%s (staged; CAL:SAVE persists)\r\n", number(coefficient.gain, a), number(coefficient.offset, b));
    for (unsigned i = 0; i < points; ++i)
        reply("RESIDUAL %u %s\r\n", i, number(y[i] - (coefficient.gain * x[i] + coefficient.offset), a));
}

/* Small bounded decimal parser; accepts optional scientific notation without
 * pulling the full libc strtof implementation into the 112 KiB image. */
static bool parse_reference(const char* text, float* value) {
    bool negative = false, digit = false;
    if (*text == '+' || *text == '-')
        negative = *text++ == '-';
    float result = 0.0f;
    unsigned count = 0;
    while (*text >= '0' && *text <= '9') {
        if (++count > 12)
            return false;
        result = result * 10.0f + (*text++ - '0');
        digit = true;
    }
    if (*text == '.') {
        ++text;
        float place = 0.1f;
        while (*text >= '0' && *text <= '9') {
            if (++count > 12)
                return false;
            result += (*text++ - '0') * place;
            place *= 0.1f;
            digit = true;
        }
    }
    if (!digit)
        return false;
    if (*text == 'E') {
        ++text;
        bool down = false;
        if (*text == '+' || *text == '-')
            down = *text++ == '-';
        if (*text < '0' || *text > '9')
            return false;
        unsigned exponent = 0;
        while (*text >= '0' && *text <= '9') {
            exponent = exponent * 10u + (unsigned)(*text++ - '0');
            if (exponent > 12)
                return false;
        }
        while (exponent--)
            result *= down ? 0.1f : 10.0f;
    }
    if (*text || !isfinite(result) || result > 1000000.0f)
        return false;
    *value = negative ? -result : result;
    return true;
}

static void command(char* text) {
    char* arg = strchr(text, ' ');
    if (arg) {
        *arg++ = '\0';
        while (*arg == ' ')
            ++arg;
    }
    if (!arg)
        arg = "";
    if (!strcmp(text, "HELP") && !*arg) {
        reply("STATUS? MEAS? RAW? CAL:SHOW? ECHO ON|OFF; IMPEDANCE 10M|HIGHZ\r\nRANGE:I 1.5A|100MA|10MA|1MA|100UA; RANGE:V 15V|6V; AUTORANGE[:I|:V] ON|OFF\r\nCAL:BEGIN V|I|BUS; CAL:CAPTURE "
              "<reference in V/A>; CAL:FIT; "
              "CAL:POINTS?\r\nCAL:SAVE; CAL:RESET; CAL:ABORT; PING\r\n");
    } else if (!strcmp(text, "PING") && !*arg)
        reply("PONG\r\n");
    else if (!strcmp(text, "ECHO") && (!strcmp(arg, "ON") || !strcmp(arg, "OFF"))) {
        echo = !strcmp(arg, "ON");
        reply("OK\r\n");
    } else if (!strcmp(text, "STATUS?") && !*arg) {
        const smu_context_t* c = smu_get_context();
        reply("STATUS state=%u faults=%lu I=%s V=%s autorange=%u autorange_v=%u impedance_requested=%s impedance=%s valid=%u frames=%lu capture=%u points=%u\r\n", c->state, (unsigned long)c->faults,
              irange_name(c->range), vrange_name(c->vrange), c->current_autorange, c->voltage_autorange, c->input_10m_requested ? "10M" : "HIGHZ", c->input_10m_active ? "10M" : "HIGHZ",
              c->measurement_valid, (unsigned long)c->frame_count, capturing, points);
    } else if ((!strcmp(text, "MEAS?") || !strcmp(text, "RAW?")) && !*arg) {
        smu_measurement_outputs_t out;
        smu_get_measurement(&out);
        const smu_measurement_t* m = &out.precision;
        char a[32], b[32], c[32];
        if (!strcmp(text, "RAW?"))
            reply("RAW I=%ld V=%ld BUS=%ld valid=%u\r\n", (long)m->adc_i_raw, (long)m->adc_v_raw, (long)m->adc_cal_raw, m->valid);
        else
            reply("MEAS I_A=%s V_V=%s BUS_V=%s I=%s V=%s valid=%u samples=%lu\r\n", number(m->current_A, a), number(m->voltage_V, b), number(m->calbus_V, c), irange_name(m->range),
                  vrange_name(m->vrange), m->valid, (unsigned long)out.sample_count);
    } else if (!strcmp(text, "CAL:ABORT") && !*arg) {
        capturing = ready = false;
        target = TARGET_NONE;
        points = 0;
        reply("OK capture aborted; staged fits retained\r\n");
    } else if (!strcmp(text, "CAL:RESET") && !*arg) {
        staged = *smu_calibration_get();
        dirty = capturing = ready = false;
        target = TARGET_NONE;
        points = 0;
        reply("OK staged calibration reset\r\n");
    } else if (!strcmp(text, "CAL:SHOW?") && !*arg)
        show_cal();
    else if (!strcmp(text, "CAL:POINTS?") && !*arg) {
        char a[32], b[32];
        for (unsigned i = 0; i < points; ++i)
            reply("POINT %u nominal=%s reference=%s\r\n", i, number(x[i], a), number(y[i], b));
        reply("OK points=%u\r\n", points);
    } else if (!strcmp(text, "CAL:FIT") && !*arg)
        fit();
    else if (!strcmp(text, "CAL:SAVE") && !*arg) {
        if (capturing || !dirty || !stable())
            reply("ERR save requires staged fit and stable fixed range\r\n");
        else if (staged.sequence != smu_calibration_get()->sequence)
            reply("ERR active calibration changed; CAL:RESET and refit\r\n");
        else if (smu_calibration_commit(&staged)) {
            staged = *smu_calibration_get();
            dirty = false;
            reply("OK saved sequence=%lu\r\n", (unsigned long)staged.sequence);
        } else
            reply("ERR flash save failed\r\n");
    } else if (!strcmp(text, "CAL:BEGIN")) {
        if (capturing || !stable() || (strcmp(arg, "V") && strcmp(arg, "I") && strcmp(arg, "BUS"))) {
            reply("ERR select settled fixed ranges with both autoranges OFF; target V|I|BUS\r\n");
            return;
        }
        target = !strcmp(arg, "V") ? TARGET_V : !strcmp(arg, "I") ? TARGET_I : TARGET_BUS;
        current_range = smu_get_context()->range;
        voltage_range = smu_get_context()->vrange;
        capture_input_10m = smu_get_context()->input_10m_active;
        points = 0;
        ready = false;
        reply("OK manual calibration; set physical reference, then CAL:CAPTURE value\r\n");
    } else if (!strcmp(text, "CAL:CAPTURE")) {
        float value;
        bool parsed = parse_reference(arg, &value);
        if (!parsed || capturing || target == TARGET_NONE || points >= POINTS || !stable() || !same_range())
            reply("ERR capture requires finite reference, active session and unchanged settled ranges\r\n");
        else {
            reference = value;
            acquired = 0;
            code_sum = 0;
            capturing = true;
            started = smu_port_millis();
            reply("OK acquiring 256 samples\r\n");
        }
    } else if (!strcmp(text, "IMPEDANCE")) {
        if (capturing) {
            reply("ERR capture busy\r\n");
            return;
        }
        if (strcmp(arg, "10M") && strcmp(arg, "HIGHZ")) {
            reply("ERR 10M|HIGHZ\r\n");
            return;
        }
        const smu_status_t result = smu_set_input_10m(!strcmp(arg, "10M"));
        if (result == SMU_OK) {
            target = TARGET_NONE;
            points = 0;
            reply("OK impedance requested; wait for STATUS valid=1\r\n");
        } else
            reply("ERR impedance request status=%u\r\n", result);
    } else if (!strcmp(text, "RANGE:I") || !strcmp(text, "RANGE:V") || !strcmp(text, "AUTORANGE") || !strcmp(text, "AUTORANGE:I") || !strcmp(text, "AUTORANGE:V")) {
        if (capturing) {
            reply("ERR capture busy\r\n");
            return;
        }
        if (!strcmp(text, "AUTORANGE") || !strcmp(text, "AUTORANGE:I") || !strcmp(text, "AUTORANGE:V")) {
            if (strcmp(arg, "ON") && strcmp(arg, "OFF"))
                reply("ERR ON|OFF\r\n");
            else {
                if (!strcmp(text, "AUTORANGE:V"))
                    smu_set_voltage_autorange(!strcmp(arg, "ON"));
                else
                    smu_set_current_autorange(!strcmp(arg, "ON"));
                target = TARGET_NONE;
                points = 0;
                reply("OK\r\n");
            }
        } else {
            smu_status_t result = SMU_ERR_ARG;
            if (!strcmp(text, "RANGE:I"))
                for (unsigned r = 1; r <= 5; ++r) {
                    if (!strcmp(arg, irange_name((smu_current_range_t)r)))
                        result = smu_set_current_range((smu_current_range_t)r);
                }
            else if (!strcmp(arg, "6V") || !strcmp(arg, "15V"))
                result = smu_set_voltage_range(!strcmp(arg, "6V") ? SMU_VRANGE_6V : SMU_VRANGE_15V);
            if (result == SMU_OK) {
                target = TARGET_NONE;
                points = 0;
                reply("OK range requested; wait for STATUS valid=1\r\n");
            } else
                reply("ERR range request status=%u\r\n", result);
        }
    } else
        reply("ERR unknown command; HELP\r\n");
}

bool smu_console_init(void) {
    staged = *smu_calibration_get();
    used = points = acquired = 0;
    target = TARGET_NONE;
    discard_line = output_lost = dirty = capturing = ready = echo = false;
    if (!smu_console_port_init())
        return false;
    reply("SMUK serial ready 115200 8N1. HELP for commands.\r\n");
    return true;
}

void smu_console_frame(const ads131m03_dma_frame_t* f, smu_current_range_t irange, smu_voltage_range_t vrange) {
    if (!capturing)
        return;
    if (irange != current_range || vrange != voltage_range || !stable() || !same_range()) {
        capturing = false;
        reply("ERR capture range/state changed\r\n");
        return;
    }
    if (smu_port_millis() - started >= CAPTURE_TIMEOUT_MS) {
        capturing = false;
        reply("ERR capture timeout; point discarded\r\n");
        return;
    }
    int32_t code = target == TARGET_I ? f->ch0 : target == TARGET_V ? f->ch1 : f->ch2;
    if (code >= 8220000 || code <= -8220000) {
        capturing = false;
        reply("ERR capture near ADC clipping; reduce reference\r\n");
        return;
    }
    code_sum += code;
    if (++acquired == CAPTURE_SAMPLES) {
        capturing = false;
        ready = true;
    }
}

void smu_console_process(void) {
    if (output_lost && smu_console_port_write("ERR TX overflow; retry query\r\n", 29))
        output_lost = false;
    if (ready) {
        ready = false;
        float code = (float)code_sum / CAPTURE_SAMPLES;
        float adc_v = code * (1.2f / 8388608.0f);
        x[points] = target == TARGET_I ? smu_current_from_adc(adc_v, current_range) : target == TARGET_V ? smu_voltage_from_adc(adc_v, voltage_range) : smu_calbus_from_adc(adc_v);
        y[points] = reference;
        char a[32], b[32];
        reply("OK POINT %u nominal=%s reference=%s\r\n", points, number(x[points], a), number(y[points], b));
        ++points;
    }
    if (capturing && smu_port_millis() - started >= CAPTURE_TIMEOUT_MS) {
        capturing = false;
        reply("ERR capture timeout; point discarded\r\n");
    }
    if (smu_console_port_rx_lost()) {
        discard_line = true;
        used = 0;
        reply("ERR RX loss; resend after newline\r\n");
    }
    uint8_t byte;
    /* Bounded work so incoming traffic cannot monopolize the main loop. */
    for (unsigned budget = 0; budget < 64 && smu_console_port_read(&byte); ++budget) {
        if (byte == '\r' || byte == '\n') {
            if (echo)
                reply("\r\n");
            if (discard_line) {
                discard_line = false;
                used = 0;
            } else if (used) {
                line[used] = '\0';
                command(line);
                used = 0;
            }
        } else if (!discard_line && (byte == 8 || byte == 127)) {
            if (used) {
                --used;
                if (echo)
                    reply("\b \b");
            }
        } else if (!discard_line && byte >= 32 && byte < 127) {
            if (used == LINE_SIZE - 1) {
                discard_line = true;
                used = 0;
                reply("ERR line too long\r\n");
            } else {
                if (echo)
                    reply("%c", byte);
                line[used++] = (char)toupper(byte);
            }
        } else if (byte != 8 && byte != 127 && byte != '\r' && byte != '\n') {
            discard_line = true;
            used = 0;
        }
    }
}
