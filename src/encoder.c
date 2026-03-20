#include "encoder.h"
#include "buttons.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "pico/stdlib.h"

// GPIO polling-based quadrature decoder
// With 50nF hardware debounce caps on CLK/DT, this is reliable at 30Hz polling.
// The KY-040 generates 4 transitions per detent, we count detents.

static uint8_t prev_state = 0;     // Previous AB state (2 bits)
static int32_t step_count = 0;     // Accumulated gray code steps

// Button debounce state (encoder push button on GP4)
#define DEBOUNCE_US       50000    // 50ms
#define SHORT_PRESS_US   500000    // 500ms
#define LONG_PRESS_US   1000000    // 1000ms

static bool     btn_state = false;
static uint64_t btn_last_change = 0;
static uint64_t btn_press_start = 0;

// Gray code direction lookup table
// Index = (prev_AB << 2) | curr_AB, value = direction (-1, 0, +1)
static const int8_t quad_table[16] = {
    // prev\curr  00   01   10   11
    /*00*/         0,  +1,  -1,   0,
    /*01*/        -1,   0,   0,  +1,
    /*10*/        +1,   0,   0,  -1,
    /*11*/         0,  -1,  +1,   0,
};

static uint8_t read_ab(void) {
    uint8_t a = gpio_get(ENCODER_CLK_PIN) ? 1 : 0;
    uint8_t b = gpio_get(ENCODER_DT_PIN) ? 1 : 0;
    return (a << 1) | b;
}

void encoder_init(void) {
    // CLK and DT as inputs with internal pull-ups
    // (KY-040 has external 10k pull-ups + 50nF debounce caps)
    gpio_init(ENCODER_CLK_PIN);
    gpio_set_dir(ENCODER_CLK_PIN, GPIO_IN);
    gpio_pull_up(ENCODER_CLK_PIN);

    gpio_init(ENCODER_DT_PIN);
    gpio_set_dir(ENCODER_DT_PIN, GPIO_IN);
    gpio_pull_up(ENCODER_DT_PIN);

    // Push button
    gpio_init(ENCODER_SW_PIN);
    gpio_set_dir(ENCODER_SW_PIN, GPIO_IN);
    gpio_pull_up(ENCODER_SW_PIN);

    prev_state = read_ab();
    step_count = 0;
}

// Call frequently (1kHz+) to catch all gray code transitions
void encoder_poll(void) {
    uint8_t curr = read_ab();
    if (curr != prev_state) {
        uint8_t idx = (prev_state << 2) | curr;
        step_count += quad_table[idx];
        prev_state = curr;
    }
}

// Returns accumulated detent clicks since last call
int encoder_get_delta(void) {
    encoder_poll();  // One more sample

    // Bare encoder: typically 2 gray code steps per detent
    int detents = step_count / 2;
    step_count -= detents * 2;  // Keep remainder

    return detents;
}

uint8_t encoder_button_poll(void) {
    uint64_t now = time_us_64();
    bool raw_pressed = !gpio_get(ENCODER_SW_PIN);  // Active low

    if (raw_pressed != btn_state) {
        if (now - btn_last_change < DEBOUNCE_US) {
            return BTN_NONE;
        }

        btn_last_change = now;
        btn_state = raw_pressed;

        if (btn_state) {
            btn_press_start = now;
        } else {
            // Released — determine press type
            uint64_t held_us = now - btn_press_start;
            if (held_us >= LONG_PRESS_US) {
                return BTN_LONG;
            } else if (held_us < SHORT_PRESS_US) {
                return BTN_SHORT;
            }
            // 500-1000ms dead zone
        }
    }

    return BTN_NONE;
}

bool encoder_button_held(void) {
    return btn_state;
}
