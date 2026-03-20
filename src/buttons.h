#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdint.h>
#include <stdbool.h>

#include "hardware_config.h"
// Toggle switch (on/off, not momentary)
#define SWITCH1_PIN  PIN_SWITCH1

// Encoder button press types (used by encoder module for SW pin)
#define BTN_NONE       0
#define BTN_SHORT      1  // < 500ms
#define BTN_LONG       2  // > 1000ms

// Initialize toggle switch on GP5
void buttons_init(void);

// Read toggle switch state (true = ON, false = OFF)
// Debounced with 50ms window
bool switch1_read(void);

// Detect toggle switch state change (returns true once per transition)
bool switch1_changed(void);

#endif
