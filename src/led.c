#include "led.h"
#include "hardware/pio.h"
#include "ws2812.pio.h"

// PIO-based WS2812 driver for single onboard LED on GP22
// Uses PIO0 SM0 — runs independently of CPU with perfect timing

static PIO led_pio;
static uint led_sm;

static void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(led_pio, led_sm, pixel_grb << 8u);
}

void led_init(void) {
    led_pio = pio0;
    led_sm = 0;
    uint offset = pio_add_program(led_pio, &ws2812_program);
    ws2812_program_init(led_pio, led_sm, offset, LED_PIN, 800000, true);
}

void led_set_rgb(uint8_t r, uint8_t g, uint8_t b) {
    // Waveshare onboard LED: R and G channels are swapped vs standard WS2812
    uint32_t grb = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    put_pixel(grb);
}

void led_linked(void) {
    led_set_rgb(0, 30, 0);  // Dim green
}

void led_no_link(void) {
    led_set_rgb(30, 0, 0);  // Dim red
}

void led_starting(void) {
    led_set_rgb(30, 15, 0);  // Dim orange
}
