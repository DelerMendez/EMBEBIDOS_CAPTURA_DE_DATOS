#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int bpm;
    float spo2;
    bool finger_present;
} signal_result_t;

void signal_proc_init(void);
void signal_proc_update(uint16_t red, uint16_t ir);
signal_result_t signal_proc_get_result(void);

#ifdef __cplusplus
}
#endif