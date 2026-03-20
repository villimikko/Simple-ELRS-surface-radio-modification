#include "mixer.h"
#include "crsf.h"
#include <math.h>

// Deadband: ~8 ADC counts / range ≈ 0.005
#define DEADBAND_THRESHOLD  0.005f

uint16_t mixer_process(uint16_t adc_val, const cal_pot_t *cal, bool spring_return) {
    // 1. Calibrate: raw ADC → [-1.0, +1.0]
    float val;
    if (adc_val <= cal->center) {
        if (cal->center == cal->min) {
            val = 0.0f;
        } else {
            val = -(float)(cal->center - adc_val) / (float)(cal->center - cal->min);
        }
    } else {
        if (cal->max == cal->center) {
            val = 0.0f;
        } else {
            val = (float)(adc_val - cal->center) / (float)(cal->max - cal->center);
        }
    }

    // 2. Reverse if needed
    if (cal->reverse) {
        val = -val;
    }

    // 3. Deadband at center (spring-return pots only)
    if (spring_return && fabsf(val) < DEADBAND_THRESHOLD) {
        val = 0.0f;
    }

    // 4. Clamp
    if (val < -1.0f) val = -1.0f;
    if (val >  1.0f) val =  1.0f;

    // 5. Map to CRSF range (172-992-1811)
    uint16_t crsf = (uint16_t)(CRSF_CHANNEL_CENTER +
                    val * (float)(CRSF_CHANNEL_MAX - CRSF_CHANNEL_CENTER));

    if (crsf < CRSF_CHANNEL_MIN) crsf = CRSF_CHANNEL_MIN;
    if (crsf > CRSF_CHANNEL_MAX) crsf = CRSF_CHANNEL_MAX;

    return crsf;
}
