#include "menu.h"
#include "buttons.h"
#include <stdio.h>
#include <string.h>

// --- Menu item counts ---
#define MAIN_MENU_ITEMS      5   // Steering, Throttle, Model Select, Calibrate, Back
#define STEER_MENU_ITEMS     7   // EPA L, EPA R, Subtrim, DR, Expo, Reverse, Back
#define THROTTLE_MENU_ITEMS  7   // EPA Fwd, EPA Brk, Subtrim, DR, Expo, Reverse, Back
#define MODEL_MENU_ITEMS     5   // Model 1-4, Back
#define DISPLAY_PAGES        3   // Driving, Big Numbers, Debug

// --- Submenu item indices ---
// Steering / Throttle share the same layout
enum {
    ITEM_EPA_A     = 0,  // EPA Left / EPA Fwd
    ITEM_EPA_B     = 1,  // EPA Right / EPA Brk
    ITEM_SUBTRIM   = 2,
    ITEM_DR        = 3,
    ITEM_EXPO      = 4,
    ITEM_REVERSE   = 5,
    ITEM_BACK      = 6,
};

// Main menu indices
enum {
    MAIN_STEERING     = 0,
    MAIN_THROTTLE     = 1,
    MAIN_MODEL_SELECT = 2,
    MAIN_CALIBRATE    = 3,
    MAIN_BACK         = 4,
};

// --- Helpers ---

static inline int clamp_int(int val, int lo, int hi) {
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

static inline void wrap_cursor(uint8_t *cursor, int delta, uint8_t count) {
    int c = (int)*cursor + delta;
    c = c % (int)count;
    if (c < 0) c += count;
    *cursor = (uint8_t)c;
}

// --- Steering/Throttle value access ---

static int get_channel_value(const model_config_t *mdl, menu_state_t state, uint8_t item) {
    bool steer = (state == MENU_STEERING);
    switch (item) {
        case ITEM_EPA_A:   return steer ? mdl->steer_epa_left    : mdl->throttle_epa_fwd;
        case ITEM_EPA_B:   return steer ? mdl->steer_epa_right   : mdl->throttle_epa_brk;
        case ITEM_SUBTRIM: return steer ? mdl->steer_subtrim     : mdl->throttle_subtrim;
        case ITEM_DR:      return steer ? mdl->steer_dr          : mdl->throttle_dr;
        case ITEM_EXPO:    return steer ? mdl->steer_expo        : mdl->throttle_expo;
        case ITEM_REVERSE: return steer ? mdl->steer_reverse     : mdl->throttle_reverse;
        default: return 0;
    }
}

static void set_channel_value(model_config_t *mdl, menu_state_t state, uint8_t item, int val) {
    bool steer = (state == MENU_STEERING);
    switch (item) {
        case ITEM_EPA_A:
            if (steer) mdl->steer_epa_left      = (uint8_t)val;
            else       mdl->throttle_epa_fwd     = (uint8_t)val;
            break;
        case ITEM_EPA_B:
            if (steer) mdl->steer_epa_right      = (uint8_t)val;
            else       mdl->throttle_epa_brk     = (uint8_t)val;
            break;
        case ITEM_SUBTRIM:
            if (steer) mdl->steer_subtrim        = (int8_t)val;
            else       mdl->throttle_subtrim     = (int8_t)val;
            break;
        case ITEM_DR:
            if (steer) mdl->steer_dr             = (uint8_t)val;
            else       mdl->throttle_dr          = (uint8_t)val;
            break;
        case ITEM_EXPO:
            if (steer) mdl->steer_expo           = (uint8_t)val;
            else       mdl->throttle_expo        = (uint8_t)val;
            break;
        case ITEM_REVERSE:
            if (steer) mdl->steer_reverse        = (bool)val;
            else       mdl->throttle_reverse     = (bool)val;
            break;
        default:
            break;
    }
}

static int item_step(uint8_t item) {
    switch (item) {
        case ITEM_EPA_A:   return 5;
        case ITEM_EPA_B:   return 5;
        case ITEM_SUBTRIM: return 1;
        case ITEM_DR:      return 5;
        case ITEM_EXPO:    return 5;
        default:           return 1;
    }
}

static int item_min(uint8_t item) {
    switch (item) {
        case ITEM_EPA_A:   return 50;
        case ITEM_EPA_B:   return 50;
        case ITEM_SUBTRIM: return -100;
        case ITEM_DR:      return 10;
        case ITEM_EXPO:    return 0;
        default:           return 0;
    }
}

static int item_max(uint8_t item) {
    switch (item) {
        case ITEM_EPA_A:   return 120;
        case ITEM_EPA_B:   return 120;
        case ITEM_SUBTRIM: return 100;
        case ITEM_DR:      return 100;
        case ITEM_EXPO:    return 100;
        default:           return 1;
    }
}

// --- Public API ---

void menu_init(menu_t *menu) {
    memset(menu, 0, sizeof(menu_t));
    menu->state        = MENU_DRIVING;
    menu->cursor       = 0;
    menu->display_page = 0;
    menu->editing      = false;
    menu->config_dirty = false;
    menu->start_calibration = false;
}

void menu_update(menu_t *menu, int encoder_delta, uint8_t encoder_btn,
                 config_t *cfg) {
    model_config_t *mdl = config_active_model(cfg);

    switch (menu->state) {

    // ----- DRIVING MODE -----
    case MENU_DRIVING:
        if (encoder_btn == BTN_SHORT) {
            menu->display_page = (menu->display_page + 1) % DISPLAY_PAGES;
        }
        if (encoder_btn == BTN_LONG) {
            menu->state  = MENU_MAIN;
            menu->cursor = 0;
        }
        break;

    // ----- MAIN MENU -----
    case MENU_MAIN:
        if (encoder_delta != 0) {
            wrap_cursor(&menu->cursor, encoder_delta, MAIN_MENU_ITEMS);
        }
        if (encoder_btn == BTN_SHORT) {
            switch (menu->cursor) {
                case MAIN_STEERING:
                    menu->state  = MENU_STEERING;
                    menu->cursor = 0;
                    menu->editing = false;
                    break;
                case MAIN_THROTTLE:
                    menu->state  = MENU_THROTTLE;
                    menu->cursor = 0;
                    menu->editing = false;
                    break;
                case MAIN_MODEL_SELECT:
                    menu->state  = MENU_MODEL_SELECT;
                    menu->cursor = cfg->active_model;
                    break;
                case MAIN_CALIBRATE:
                    menu->state = MENU_CALIBRATING;
                    menu->start_calibration = true;
                    break;
                case MAIN_BACK:
                    menu->state  = MENU_DRIVING;
                    menu->cursor = 0;
                    break;
            }
        }
        // Long press = back to driving
        if (encoder_btn == BTN_LONG) {
            menu->state  = MENU_DRIVING;
            menu->cursor = 0;
        }
        break;

    // ----- STEERING / THROTTLE SUBMENUS -----
    case MENU_STEERING:
    case MENU_THROTTLE: {
        uint8_t count = (menu->state == MENU_STEERING) ? STEER_MENU_ITEMS : THROTTLE_MENU_ITEMS;

        if (menu->editing) {
            // Encoder rotates the value
            if (encoder_delta != 0) {
                if (menu->cursor == ITEM_REVERSE) {
                    bool cur = (bool)get_channel_value(mdl, menu->state, ITEM_REVERSE);
                    set_channel_value(mdl, menu->state, ITEM_REVERSE, !cur);
                } else {
                    int step = item_step(menu->cursor);
                    int val  = get_channel_value(mdl, menu->state, menu->cursor);
                    val = clamp_int(val + encoder_delta * step,
                                    item_min(menu->cursor),
                                    item_max(menu->cursor));
                    set_channel_value(mdl, menu->state, menu->cursor, val);
                }
            }
            // Click confirms edit
            if (encoder_btn == BTN_SHORT) {
                menu->editing = false;
                menu->config_dirty = true;
            }
            // Long press cancels edit and goes back
            if (encoder_btn == BTN_LONG) {
                menu->editing = false;
                menu->config_dirty = true;
                menu->state  = MENU_MAIN;
                menu->cursor = 0;
            }
        } else {
            // Not editing — encoder navigates
            if (encoder_delta != 0) {
                wrap_cursor(&menu->cursor, encoder_delta, count);
            }
            if (encoder_btn == BTN_SHORT) {
                if (menu->cursor == ITEM_BACK) {
                    menu->state  = MENU_MAIN;
                    menu->cursor = 0;
                } else if (menu->cursor == ITEM_REVERSE) {
                    // Toggle reverse directly
                    bool cur = (bool)get_channel_value(mdl, menu->state, ITEM_REVERSE);
                    set_channel_value(mdl, menu->state, ITEM_REVERSE, !cur);
                    menu->config_dirty = true;
                } else {
                    menu->editing = true;
                }
            }
            // Long press = back to main menu
            if (encoder_btn == BTN_LONG) {
                menu->state  = MENU_MAIN;
                menu->cursor = 0;
            }
        }
        break;
    }

    // ----- MODEL SELECT -----
    case MENU_MODEL_SELECT:
        if (encoder_delta != 0) {
            wrap_cursor(&menu->cursor, encoder_delta, MODEL_MENU_ITEMS);
        }
        if (encoder_btn == BTN_SHORT) {
            if (menu->cursor < CONFIG_MAX_MODELS) {
                cfg->active_model = menu->cursor;
                menu->config_dirty = true;
                menu->state  = MENU_DRIVING;
                menu->cursor = 0;
            } else {
                // Back item
                menu->state  = MENU_MAIN;
                menu->cursor = 0;
            }
        }
        if (encoder_btn == BTN_LONG) {
            menu->state  = MENU_MAIN;
            menu->cursor = 0;
        }
        break;

    // ----- CALIBRATING -----
    case MENU_CALIBRATING:
        // Calibration module drives the state.
        // When done, it sets menu->state = MENU_DRIVING.
        break;
    }
}

uint8_t menu_item_count(const menu_t *menu) {
    switch (menu->state) {
        case MENU_MAIN:         return MAIN_MENU_ITEMS;
        case MENU_STEERING:     return STEER_MENU_ITEMS;
        case MENU_THROTTLE:     return THROTTLE_MENU_ITEMS;
        case MENU_MODEL_SELECT: return MODEL_MENU_ITEMS;
        default:                return 0;
    }
}

void menu_item_text(const menu_t *menu, uint8_t index,
                    const config_t *cfg, char *label_buf, char *value_buf) {
    label_buf[0] = '\0';
    value_buf[0] = '\0';

    const model_config_t *mdl = &cfg->models[cfg->active_model < CONFIG_MAX_MODELS
                                              ? cfg->active_model : 0];

    switch (menu->state) {

    case MENU_MAIN:
        switch (index) {
            case MAIN_STEERING:     strcpy(label_buf, "Steering");     break;
            case MAIN_THROTTLE:     strcpy(label_buf, "Throttle");     break;
            case MAIN_MODEL_SELECT: strcpy(label_buf, "Model Select"); break;
            case MAIN_CALIBRATE:    strcpy(label_buf, "Calibrate");    break;
            case MAIN_BACK:         strcpy(label_buf, "Back");         break;
        }
        break;

    case MENU_STEERING:
        switch (index) {
            case ITEM_EPA_A:
                strcpy(label_buf, "EPA Left");
                snprintf(value_buf, 16, "%u%%", mdl->steer_epa_left);
                break;
            case ITEM_EPA_B:
                strcpy(label_buf, "EPA Right");
                snprintf(value_buf, 16, "%u%%", mdl->steer_epa_right);
                break;
            case ITEM_SUBTRIM:
                strcpy(label_buf, "Subtrim");
                snprintf(value_buf, 16, "%+d", mdl->steer_subtrim);
                break;
            case ITEM_DR:
                strcpy(label_buf, "Dual Rate");
                snprintf(value_buf, 16, "%u%%", mdl->steer_dr);
                break;
            case ITEM_EXPO:
                strcpy(label_buf, "Expo");
                snprintf(value_buf, 16, "%u%%", mdl->steer_expo);
                break;
            case ITEM_REVERSE:
                strcpy(label_buf, "Reverse");
                strcpy(value_buf, mdl->steer_reverse ? "Yes" : "No");
                break;
            case ITEM_BACK:
                strcpy(label_buf, "Back");
                break;
        }
        break;

    case MENU_THROTTLE:
        switch (index) {
            case ITEM_EPA_A:
                strcpy(label_buf, "EPA Fwd");
                snprintf(value_buf, 16, "%u%%", mdl->throttle_epa_fwd);
                break;
            case ITEM_EPA_B:
                strcpy(label_buf, "EPA Brk");
                snprintf(value_buf, 16, "%u%%", mdl->throttle_epa_brk);
                break;
            case ITEM_SUBTRIM:
                strcpy(label_buf, "Subtrim");
                snprintf(value_buf, 16, "%+d", mdl->throttle_subtrim);
                break;
            case ITEM_DR:
                strcpy(label_buf, "Dual Rate");
                snprintf(value_buf, 16, "%u%%", mdl->throttle_dr);
                break;
            case ITEM_EXPO:
                strcpy(label_buf, "Expo");
                snprintf(value_buf, 16, "%u%%", mdl->throttle_expo);
                break;
            case ITEM_REVERSE:
                strcpy(label_buf, "Reverse");
                strcpy(value_buf, mdl->throttle_reverse ? "Yes" : "No");
                break;
            case ITEM_BACK:
                strcpy(label_buf, "Back");
                break;
        }
        break;

    case MENU_MODEL_SELECT:
        if (index < CONFIG_MAX_MODELS) {
            snprintf(label_buf, 24, "%s", cfg->models[index].name);
            if (index == cfg->active_model) {
                strcpy(value_buf, "[active]");
            }
        } else {
            strcpy(label_buf, "Back");
        }
        break;

    default:
        break;
    }
}
