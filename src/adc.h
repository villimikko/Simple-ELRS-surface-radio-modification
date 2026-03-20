#ifndef ADC_H
#define ADC_H

#include <stdint.h>

// ADC channels for pots (derived from pin numbers in hardware_config.h)
// GP26=ch0, GP27=ch1, GP28=ch2, GP29=ch3
#include "hardware_config.h"
#define ADC_CH_THROTTLE     (PIN_POT_THROTTLE - 26)
#define ADC_CH_STEERING     (PIN_POT_STEERING - 26)
#define ADC_CH_TRIM_STEER   (PIN_POT_TRIM_STEER - 26)
#define ADC_CH_TRIM_THROT   (PIN_POT_TRIM_THROT - 26)

#define ADC_OVERSAMPLE      16
#define ADC_NUM_CHANNELS    4

// EMA smoothing factor (0.0-1.0, lower = more smoothing)
#define ADC_EMA_ALPHA       0.3f

// Deadband in ADC counts (applied at center for spring-return pots)
#define ADC_DEADBAND        8

// Initialize ADC for 4 channels (GP26-GP29)
void adc_inputs_init(void);

// Read all 4 channels with 16x oversampling + EMA smoothing
// Results stored in adc_smoothed[] (0-4095 range)
void adc_read_all(void);

// Get smoothed ADC value for a channel (0-4095)
uint16_t adc_get(uint8_t channel);

// Get raw (pre-smoothing) value for debug display
uint16_t adc_get_raw(uint8_t channel);

#endif
