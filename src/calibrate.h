#ifndef CALIBRATE_H
#define CALIBRATE_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

typedef enum {
    CAL_IDLE,
    CAL_SAMPLING,   // Tracking min/max as user moves pots
    CAL_DONE,
} cal_state_t;

typedef struct {
    cal_state_t state;
    cal_pot_t results[4];       // min/center/max per pot
    uint16_t live_adc[4];       // Current ADC values for display
} calibrate_t;

// Start calibration: sample centers from current pot positions (must be centered)
void calibrate_start(calibrate_t *cal);

// Call each frame (~15Hz). Tracks min/max for all pots.
// encoder_click: user pressed encoder to finish calibration
// Returns true while calibration is in progress.
bool calibrate_update(calibrate_t *cal, bool encoder_click);

// Copy results into config
void calibrate_apply(const calibrate_t *cal, config_t *cfg);

#endif
