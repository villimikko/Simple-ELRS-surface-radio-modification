#ifndef MIXER_H
#define MIXER_H

#include <stdint.h>
#include "config.h"

// Calibrate ADC → CRSF range (172-992-1811)
// spring_return: true = apply deadband at center (steering/throttle sticks)
uint16_t mixer_process(uint16_t adc_val, const cal_pot_t *cal, bool spring_return);

#endif
