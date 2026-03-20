#ifndef LED_H
#define LED_H

#include <stdint.h>

#include "hardware_config.h"
// Onboard WS2812 RGB LED
#define LED_PIN  PIN_LED_WS2812

// Initialize the LED GPIO (bit-bang WS2812 for V1, PIO later)
void led_init(void);

// Set LED color (GRB format as per WS2812)
void led_set_rgb(uint8_t r, uint8_t g, uint8_t b);

// Convenience: green = linked, red = no link, orange = starting
void led_linked(void);
void led_no_link(void);
void led_starting(void);

#endif
