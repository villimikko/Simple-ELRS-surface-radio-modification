#include "adc.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"

static float    ema_values[ADC_NUM_CHANNELS] = {0};
static uint16_t raw_values[ADC_NUM_CHANNELS] = {0};
static bool     initialized = false;

void adc_inputs_init(void) {
    adc_init();

    // GP26-GP29 = ADC0-ADC3
    adc_gpio_init(26);
    adc_gpio_init(27);
    adc_gpio_init(28);
    adc_gpio_init(29);

    // Prime EMA with initial readings
    for (int ch = 0; ch < ADC_NUM_CHANNELS; ch++) {
        adc_select_input(ch);
        uint32_t sum = 0;
        for (int i = 0; i < ADC_OVERSAMPLE; i++) {
            sum += adc_read();
        }
        ema_values[ch] = (float)(sum / ADC_OVERSAMPLE);
    }

    initialized = true;
}

void adc_read_all(void) {
    for (int ch = 0; ch < ADC_NUM_CHANNELS; ch++) {
        adc_select_input(ch);

        // 16x oversampling
        uint32_t sum = 0;
        for (int i = 0; i < ADC_OVERSAMPLE; i++) {
            sum += adc_read();
        }
        uint16_t avg = sum / ADC_OVERSAMPLE;
        raw_values[ch] = avg;

        // EMA smoothing
        ema_values[ch] = ADC_EMA_ALPHA * (float)avg
                       + (1.0f - ADC_EMA_ALPHA) * ema_values[ch];
    }
}

uint16_t adc_get(uint8_t channel) {
    if (channel >= ADC_NUM_CHANNELS) return 2048;
    return (uint16_t)(ema_values[channel] + 0.5f);
}

uint16_t adc_get_raw(uint8_t channel) {
    if (channel >= ADC_NUM_CHANNELS) return 2048;
    return raw_values[channel];
}
