// Debug PWM Reader for RP2350
// Reads PWM pulse widths on GP0 (CH1) and GP1 (CH2)
// Outputs: $P,ch1_us,ch2_us over USB CDC at 10Hz

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "tusb.h"

#define PWM_PIN_CH1  0
#define PWM_PIN_CH2  1
#define NUM_CHANNELS 2

static volatile uint64_t rise_time[NUM_CHANNELS];
static volatile uint32_t pulse_us[NUM_CHANNELS];

static void gpio_callback(uint gpio, uint32_t events) {
    int ch = -1;
    if (gpio == PWM_PIN_CH1) ch = 0;
    else if (gpio == PWM_PIN_CH2) ch = 1;
    if (ch < 0) return;

    uint64_t now = time_us_64();

    if (events & GPIO_IRQ_EDGE_RISE) {
        rise_time[ch] = now;
    }
    if (events & GPIO_IRQ_EDGE_FALL) {
        if (rise_time[ch] > 0) {
            uint64_t width = now - rise_time[ch];
            if (width > 500 && width < 3000) {  // valid PWM range
                pulse_us[ch] = (uint32_t)width;
            }
        }
    }
}

int main(void) {
    stdio_init_all();
    sleep_ms(2000);
    printf("Debug PWM Reader\n");
    printf("GP0=CH1(steering), GP1=CH2(throttle)\n");

    // Init PWM input pins
    gpio_init(PWM_PIN_CH1);
    gpio_set_dir(PWM_PIN_CH1, GPIO_IN);
    gpio_pull_down(PWM_PIN_CH1);

    gpio_init(PWM_PIN_CH2);
    gpio_set_dir(PWM_PIN_CH2, GPIO_IN);
    gpio_pull_down(PWM_PIN_CH2);

    // Edge interrupts on both pins
    gpio_set_irq_enabled_with_callback(PWM_PIN_CH1,
        GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled(PWM_PIN_CH2,
        GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);

    uint64_t next_print = time_us_64();

    while (true) {
        uint64_t now = time_us_64();

        if (now >= next_print) {
            next_print += 100000;  // 10Hz

            uint32_t ch1 = pulse_us[0];
            uint32_t ch2 = pulse_us[1];

            printf("$P,%lu,%lu\n", ch1, ch2);
        }

        tud_task();
    }

    return 0;
}
