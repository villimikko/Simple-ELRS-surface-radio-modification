#ifndef OLED_H
#define OLED_H

#include <stdint.h>
#include <stdbool.h>

#define OLED_WIDTH   128
#define OLED_HEIGHT  64

// Initialize SSD1306 on I2C0 (shared bus with INA219, must be init'd first)
// Returns true if OLED responds
bool oled_init(void);

// Clear the display buffer
void oled_clear(void);

// Draw text at pixel position (6x8 font)
void oled_text(uint8_t x, uint8_t y, const char *str);

// Draw a horizontal bar: x,y position, width, fill percentage (0-100)
void oled_hbar(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t fill_pct);

// Flush buffer to display
void oled_update(void);

#endif
