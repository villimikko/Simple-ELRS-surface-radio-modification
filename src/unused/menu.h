#ifndef MENU_H
#define MENU_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

typedef enum {
    MENU_DRIVING,       // Normal driving mode (show pages)
    MENU_MAIN,          // Main menu
    MENU_STEERING,      // Steering submenu
    MENU_THROTTLE,      // Throttle submenu
    MENU_MODEL_SELECT,  // Model selection
    MENU_CALIBRATING,   // Calibration wizard active
} menu_state_t;

typedef struct {
    menu_state_t state;
    uint8_t cursor;         // Current highlighted item
    uint8_t display_page;   // 0=driving, 1=big numbers, 2=debug (for MENU_DRIVING)
    bool editing;           // True when rotating encoder changes a value, not cursor
    bool config_dirty;      // True when a setting was changed (need to save)
    bool start_calibration; // Flag for main loop to start calibration
} menu_t;

// Initialize menu state
void menu_init(menu_t *menu);

// Process input: encoder delta and encoder button event
// button1 is removed — GP5 is now a toggle switch (handled separately)
// Encoder long press = enter/exit menu, encoder short press = select/cycle pages
void menu_update(menu_t *menu, int encoder_delta, uint8_t encoder_btn,
                 config_t *cfg);

// Get number of items in current menu (for display rendering)
uint8_t menu_item_count(const menu_t *menu);

// Get display string for a menu item at index
// Returns item label. value_buf gets filled with the current value string (e.g., "100%")
void menu_item_text(const menu_t *menu, uint8_t index,
                    const config_t *cfg, char *label_buf, char *value_buf);

#endif
