#include "signal_proc.h"

#include <math.h>
#include <string.h>

#include "esp_timer.h"
#include "app_config.h"

static uint16_t ir_buffer[SAMPLE_BUFFER_SIZE];
static uint16_t red_buffer[SAMPLE_BUFFER_SIZE];
static int buffer_index = 0;
static bool buffer_full = false;

static float ir_avg = 0.0f;
static uint16_t prev_ir = 0;
static bool prev_rising = false;
static int64_t last_beat_ms = 0;

static signal_result_t s_result = {
    .bpm = 0,
    .spo2 = -1.0f,
    .finger_present = false
};

static void clear_measurements(void)
{
    s_result.bpm = 0;
    s_result.spo2 = -1.0f;
    last_beat_ms = 0;
    prev_ir = 0;
    prev_rising = false;
    ir_avg = 0.0f;
    buffer_index = 0;
    buffer_full = false;
    memset(ir_buffer, 0, sizeof(ir_buffer));
    memset(red_buffer, 0, sizeof(red_buffer));
}

static float average_u16(const uint16_t *data, int size)
{
    if (size <= 0) return 0.0f;

    double sum = 0.0;
    for (int i = 0; i < size; i++) {
        sum += data[i];
    }
    return (float)(sum / size);
}

static float rms_ac_component(const uint16_t *data, int size, float dc)
{
    if (size <= 0) return 0.0f;

    double sum_sq = 0.0;
    for (int i = 0; i < size; i++) {
        double x = (double)data[i] - (double)dc;
        sum_sq += x * x;
    }
    return (float)sqrt(sum_sq / size);
}

static float calculate_spo2_estimate(void)
{
    int n = buffer_full ? SAMPLE_BUFFER_SIZE : buffer_index;
    if (n < 30) return -1.0f;

    float ir_dc = average_u16(ir_buffer, n);
    float red_dc = average_u16(red_buffer, n);

    if (ir_dc < 1000.0f || red_dc < 1000.0f) return -1.0f;

    float ir_ac = rms_ac_component(ir_buffer, n, ir_dc);
    float red_ac = rms_ac_component(red_buffer, n, red_dc);

    if (ir_ac < 1.0f || red_ac < 1.0f) return -1.0f;

    float r = (red_ac / red_dc) / (ir_ac / ir_dc);
    float spo2 = 110.0f - 25.0f * r;

    if (spo2 > 100.0f) spo2 = 100.0f;
    if (spo2 < 0.0f) spo2 = 0.0f;

    return spo2;
}

static void push_sample(uint16_t red, uint16_t ir)
{
    red_buffer[buffer_index] = red;
    ir_buffer[buffer_index] = ir;

    buffer_index++;
    if (buffer_index >= SAMPLE_BUFFER_SIZE) {
        buffer_index = 0;
        buffer_full = true;
    }
}

static void update_finger_presence(uint16_t red, uint16_t ir)
{
    bool now_present = (ir > 3000 && red > 3000);

    if (now_present != s_result.finger_present) {
        s_result.finger_present = now_present;
        clear_measurements();
        s_result.finger_present = now_present;
    }
}

static void update_bpm(uint16_t ir)
{
    if (!s_result.finger_present) {
        s_result.bpm = 0;
        return;
    }

    if (ir_avg <= 0.0f) {
        ir_avg = (float)ir;
    } else {
        ir_avg = 0.98f * ir_avg + 0.02f * (float)ir;
    }

    bool rising = (ir > prev_ir);

    if (prev_rising && !rising) {
        float threshold = ir_avg + 80.0f;

        if ((float)prev_ir > threshold) {
            int64_t now_ms = esp_timer_get_time() / 1000;

            if (last_beat_ms > 0) {
                int64_t diff = now_ms - last_beat_ms;
                if (diff > 400 && diff < 1500) {
                    int new_bpm = (int)(60000 / diff);
                    s_result.bpm = (s_result.bpm == 0) ? new_bpm : (s_result.bpm * 3 + new_bpm) / 4;
                }
            }

            last_beat_ms = now_ms;
        }
    }

    prev_rising = rising;
    prev_ir = ir;
}

void signal_proc_init(void)
{
    clear_measurements();
}

void signal_proc_update(uint16_t red, uint16_t ir)
{
    update_finger_presence(red, ir);

    if (s_result.finger_present) {
        push_sample(red, ir);
        update_bpm(ir);
        s_result.spo2 = calculate_spo2_estimate();
    } else {
        s_result.bpm = 0;
        s_result.spo2 = -1.0f;
    }
}

signal_result_t signal_proc_get_result(void)
{
    return s_result;
}