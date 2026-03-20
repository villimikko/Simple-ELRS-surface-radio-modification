#include "calibrate.h"
#include "adc.h"
#include <string.h>

#define CAL_CENTER_SAMPLES  64

void calibrate_start(calibrate_t *cal) {
    memset(cal, 0, sizeof(*cal));

    // Sample centers: pots should be at rest position right now
    // Average 64 samples per pot for accuracy
    uint32_t sums[4] = {0};
    for (int s = 0; s < CAL_CENTER_SAMPLES; s++) {
        adc_read_all();
        sums[0] += adc_get(ADC_CH_STEERING);
        sums[1] += adc_get(ADC_CH_THROTTLE);
        sums[2] += adc_get(ADC_CH_TRIM_STEER);
        sums[3] += adc_get(ADC_CH_TRIM_THROT);
    }

    for (int i = 0; i < 4; i++) {
        uint16_t center = (uint16_t)(sums[i] / CAL_CENTER_SAMPLES);
        cal->results[i].center = center;
        cal->results[i].min = center;  // Will be updated as user moves pots
        cal->results[i].max = center;
    }

    cal->state = CAL_SAMPLING;
}

bool calibrate_update(calibrate_t *cal, bool encoder_click) {
    if (cal->state != CAL_SAMPLING) return false;

    // Read all pots
    adc_read_all();
    uint16_t vals[4] = {
        adc_get(ADC_CH_STEERING),
        adc_get(ADC_CH_THROTTLE),
        adc_get(ADC_CH_TRIM_STEER),
        adc_get(ADC_CH_TRIM_THROT),
    };

    // Track min/max
    for (int i = 0; i < 4; i++) {
        cal->live_adc[i] = vals[i];
        if (vals[i] < cal->results[i].min) cal->results[i].min = vals[i];
        if (vals[i] > cal->results[i].max) cal->results[i].max = vals[i];
    }

    // Encoder click = done
    if (encoder_click) {
        cal->state = CAL_DONE;
        return false;
    }

    return true;
}

void calibrate_apply(const calibrate_t *cal, config_t *cfg) {
    for (int i = 0; i < 4; i++) {
        cfg->cal[i] = cal->results[i];
    }
}
