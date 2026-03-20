#ifndef ENCODER_H
#define ENCODER_H

#include <stdint.h>
#include <stdbool.h>

#include "hardware_config.h"
#define ENCODER_CLK_PIN  PIN_ENC_CLK
#define ENCODER_DT_PIN   PIN_ENC_DT
#define ENCODER_SW_PIN   PIN_ENC_SW

// Initialize encoder on GP2/GP3 (GPIO polling), push button on GP4
void encoder_init(void);

// Call frequently (1kHz+) to catch gray code transitions
void encoder_poll(void);

// Get encoder rotation delta since last call
// Positive = CW, negative = CCW
// Divided by 4 (one detent = 4 gray code steps)
int encoder_get_delta(void);

// Poll encoder push button with 50ms debounce
// Returns: BTN_NONE(0), BTN_SHORT(1), BTN_LONG(2)
uint8_t encoder_button_poll(void);

// Is the encoder button currently held down?
bool encoder_button_held(void);

#endif
