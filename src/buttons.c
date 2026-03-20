#include "buttons.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"

// Debounce time in microseconds
#define DEBOUNCE_US  50000  // 50ms

static bool     sw_state;          // Debounced switch state (true = ON)
static bool     sw_changed;        // State changed since last check
static uint64_t last_change_us;    // Timestamp of last valid state change

void buttons_init(void) {
    gpio_init(SWITCH1_PIN);
    gpio_set_dir(SWITCH1_PIN, GPIO_IN);
    gpio_pull_up(SWITCH1_PIN);

    // Read initial state
    sw_state      = !gpio_get(SWITCH1_PIN);  // Active low
    sw_changed    = false;
    last_change_us = 0;
}

bool switch1_read(void) {
    uint64_t now = time_us_64();
    bool raw = !gpio_get(SWITCH1_PIN);  // Active low

    if (raw != sw_state && (now - last_change_us >= DEBOUNCE_US)) {
        last_change_us = now;
        sw_state = raw;
        sw_changed = true;
    }

    return sw_state;
}

bool switch1_changed(void) {
    // Call switch1_read() first to update state
    switch1_read();
    if (sw_changed) {
        sw_changed = false;
        return true;
    }
    return false;
}
