#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

// ST7789 172x320 in landscape mode (MADCTL rotation)
#define DISPLAY_WIDTH   320
#define DISPLAY_HEIGHT  172

// Font dimensions (8x16 monospace bitmap)
#define FONT_WIDTH      8
#define FONT_HEIGHT     16

// RGB565 color defines
#define COLOR_BLACK     0x0000
#define COLOR_WHITE     0xFFFF
#define COLOR_RED       0xF800
#define COLOR_GREEN     0x07E0
#define COLOR_BLUE      0x001F
#define COLOR_YELLOW    0xFFE0
#define COLOR_CYAN      0x07FF
#define COLOR_ORANGE    0xFD20
#define COLOR_GRAY      0x7BEF
#define COLOR_DARK_GRAY 0x39E7

// Convert 8-bit RGB to RGB565
static inline uint16_t display_color565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// Initialize SPI0, GPIO pins, ST7789 controller, backlight on
void display_init(void);

// Set pixel write window (inclusive coordinates)
void display_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

// Send pixel buffer via SPI DMA (16-bit RGB565, count = number of pixels)
void display_send_pixels(uint16_t *buf, uint32_t count);

// Fill a rectangle with a solid color
void display_fill_rect(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h, uint16_t color);

// Fill entire screen with a solid color
void display_fill(uint16_t color);

// Draw a single 8x16 character at pixel position (x, y)
void display_draw_char(uint16_t x, uint16_t y, char ch, uint16_t fg, uint16_t bg);

// Draw a null-terminated string at pixel position (x, y)
void display_draw_string(uint16_t x, uint16_t y, const char *str, uint16_t fg, uint16_t bg);

// Draw a horizontal bar (for channel visualization)
// fill_pct: 0-100, represents how much of the bar is filled
void display_draw_hbar(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                       uint8_t fill_pct, uint16_t fg, uint16_t bg);

#endif
