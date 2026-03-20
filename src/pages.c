#include "pages.h"
#include "display.h"
#include "menu.h"
#include "crsf.h"
#include <stdio.h>
#include <string.h>

// Draw a full-width text row (pads with spaces to avoid stale pixels)
#define ROW_CHARS  (DISPLAY_WIDTH / FONT_WIDTH)  // 320/8 = 40 chars

static void draw_row(uint16_t x, uint16_t y, const char *text, uint16_t fg, uint16_t bg) {
    char padded[ROW_CHARS + 1];
    int chars_from_x = (DISPLAY_WIDTH - x) / FONT_WIDTH;
    if (chars_from_x > ROW_CHARS) chars_from_x = ROW_CHARS;
    snprintf(padded, chars_from_x + 1, "%-*s", chars_from_x, text);
    display_draw_string(x, y, padded, fg, bg);
}

// Status bar at top of screen (shared across driving pages)
static void draw_status_bar(const shared_state_t *state, const config_t *cfg) {
    // Draw full status bar background via padded text
    char bar[ROW_CHARS + 1];
    memset(bar, ' ', ROW_CHARS);
    bar[ROW_CHARS] = '\0';

    const model_config_t *mdl = &cfg->models[cfg->active_model];

    // Build status bar: model name left, link middle, battery right
    char model_str[16], link_str[12], batt_str[8];
    snprintf(model_str, sizeof(model_str), "M%d:%s", cfg->active_model + 1, mdl->name);

    if (state->link_active) {
        snprintf(link_str, sizeof(link_str), "LQ:%u%%", state->lq);
    } else {
        snprintf(link_str, sizeof(link_str), "NO LINK");
    }

    if (state->ina219_present) {
        snprintf(batt_str, sizeof(batt_str), "%u.%uV",
                 state->tx_voltage_mv / 1000,
                 (state->tx_voltage_mv % 1000) / 100);
    } else {
        batt_str[0] = '\0';
    }

    // Paint full bar background
    display_fill_rect(0, 0, DISPLAY_WIDTH, 16, COLOR_DARK_GRAY);

    // Overlay text (no flicker since bg is already painted in one pass)
    display_draw_string(4, 0, model_str, COLOR_WHITE, COLOR_DARK_GRAY);
    display_draw_string(180, 0, link_str,
                        state->link_active ? COLOR_GREEN : COLOR_RED,
                        COLOR_DARK_GRAY);
    if (batt_str[0]) {
        display_draw_string(264, 0, batt_str, COLOR_WHITE, COLOR_DARK_GRAY);
    }
}

// Page 1: Driving view with channel bars and telemetry
static void draw_page_driving(const shared_state_t *state, const config_t *cfg) {
    draw_status_bar(state, cfg);

    char buf[42];
    uint16_t y = 20;

    // Steering bar (label + bar + value — all elements cover their areas)
    display_draw_string(4, y, "STR", COLOR_WHITE, COLOR_BLACK);
    int steer_pct = ((int)state->ch_values[0] - CRSF_CHANNEL_CENTER) * 100
                    / (CRSF_CHANNEL_MAX - CRSF_CHANNEL_CENTER);
    uint8_t steer_fill = 50 + steer_pct / 2;
    if (steer_fill > 100) steer_fill = 100;
    display_draw_hbar(36, y, 220, 14, steer_fill, COLOR_CYAN, COLOR_DARK_GRAY);
    snprintf(buf, sizeof(buf), "%-7s", "");  // clear area first
    snprintf(buf, sizeof(buf), "%+d%%  ", steer_pct);
    display_draw_string(264, y, buf, COLOR_WHITE, COLOR_BLACK);

    y += 22;

    // Throttle bar
    display_draw_string(4, y, "THR", COLOR_WHITE, COLOR_BLACK);
    int thr_pct = ((int)state->ch_values[1] - CRSF_CHANNEL_CENTER) * 100
                  / (CRSF_CHANNEL_MAX - CRSF_CHANNEL_CENTER);
    uint8_t thr_fill = 50 + thr_pct / 2;
    if (thr_fill > 100) thr_fill = 100;
    display_draw_hbar(36, y, 220, 14, thr_fill, COLOR_GREEN, COLOR_DARK_GRAY);
    snprintf(buf, sizeof(buf), "%+d%%  ", thr_pct);
    display_draw_string(264, y, buf, COLOR_WHITE, COLOR_BLACK);

    y += 26;

    // Separator
    display_fill_rect(0, y, DISPLAY_WIDTH, 1, COLOR_GRAY);
    y += 4;

    // Telemetry row 1
    if (state->link_active) {
        snprintf(buf, sizeof(buf), "RSSI %ddBm   LQ %u%%   SNR %d",
                 state->rssi, state->lq, state->snr);
    } else {
        snprintf(buf, sizeof(buf), "RSSI --      LQ --     SNR --");
    }
    draw_row(4, y, buf, COLOR_WHITE, COLOR_BLACK);
    y += 18;

    // Telemetry row 2
    if (state->vbat_mv > 0) {
        snprintf(buf, sizeof(buf), "CAR %u.%uV",
                 state->vbat_mv / 1000, (state->vbat_mv % 1000) / 100);
    } else {
        snprintf(buf, sizeof(buf), "CAR --");
    }
    if (state->ina219_present) {
        char tmp[20];
        snprintf(tmp, sizeof(tmp), "  TX %u.%uV %dmA",
                 state->tx_voltage_mv / 1000,
                 (state->tx_voltage_mv % 1000) / 100,
                 state->tx_current_ma);
        strncat(buf, tmp, sizeof(buf) - strlen(buf) - 1);
    }
    draw_row(4, y, buf, COLOR_WHITE, COLOR_BLACK);
    y += 18;

    // Uptime
    uint32_t s = state->uptime_seconds;
    snprintf(buf, sizeof(buf), "%02u:%02u:%02u  TX#%lu",
             s / 3600, (s / 60) % 60, s % 60, state->crsf_tx_count);
    draw_row(4, y, buf, COLOR_GRAY, COLOR_BLACK);
}

// Page 2: Big numbers (link quality focus)
static void draw_page_big_numbers(const shared_state_t *state, const config_t *cfg) {
    draw_status_bar(state, cfg);

    char buf[32];
    uint16_t y = 24;

    draw_row(100, y, "LINK STATUS", COLOR_WHITE, COLOR_BLACK);
    y += 24;

    if (state->link_active) {
        snprintf(buf, sizeof(buf), "LQ:  %u%%", state->lq);
        draw_row(60, y, buf, state->lq > 70 ? COLOR_GREEN : COLOR_RED, COLOR_BLACK);
        y += 20;

        snprintf(buf, sizeof(buf), "RSSI: %d", state->rssi);
        draw_row(60, y, buf, COLOR_WHITE, COLOR_BLACK);
        y += 20;

        snprintf(buf, sizeof(buf), "SNR:  %d", state->snr);
        draw_row(60, y, buf, COLOR_WHITE, COLOR_BLACK);
        y += 24;
    } else {
        draw_row(80, y, "NO LINK", COLOR_RED, COLOR_BLACK);
        y += 64;
    }

    if (state->vbat_mv > 0) {
        snprintf(buf, sizeof(buf), "VBAT: %u.%uV",
                 state->vbat_mv / 1000, (state->vbat_mv % 1000) / 100);
        draw_row(60, y, buf, COLOR_YELLOW, COLOR_BLACK);
    }
}

// Page 3: Debug (raw ADC, CRSF stats)
static void draw_page_debug(const shared_state_t *state, const config_t *cfg) {
    (void)cfg;
    char buf[48];
    uint16_t y = 0;

    draw_row(4, y, "DEBUG", COLOR_YELLOW, COLOR_BLACK);
    y += 18;

    // ADC values
    const char *names[] = {"THR", "STR", "S.T", "T.T"};
    for (int i = 0; i < 4; i++) {
        snprintf(buf, sizeof(buf), "ADC%d(%s): %4u  CH%d: %4u",
                 i, names[i], state->raw_adc[i],
                 i + 1, i < 2 ? state->ch_values[i] : 0);
        draw_row(4, y, buf, COLOR_WHITE, COLOR_BLACK);
        y += 16;
    }

    y += 4;

    snprintf(buf, sizeof(buf), "CRSF TX: %lu  RX: %lu",
             state->crsf_tx_count, state->crsf_rx_count);
    draw_row(4, y, buf, COLOR_WHITE, COLOR_BLACK);
    y += 18;

    if (state->ina219_present) {
        snprintf(buf, sizeof(buf), "TX: %u.%02uV  %dmA  %umW",
                 state->tx_voltage_mv / 1000,
                 (state->tx_voltage_mv % 1000) / 10,
                 state->tx_current_ma,
                 state->tx_power_mw);
        draw_row(4, y, buf, COLOR_WHITE, COLOR_BLACK);
        y += 18;
    }

    uint32_t s = state->uptime_seconds;
    snprintf(buf, sizeof(buf), "Up: %02u:%02u:%02u  FW: v0.2.0",
             s / 3600, (s / 60) % 60, s % 60);
    draw_row(4, y, buf, COLOR_GRAY, COLOR_BLACK);
}

// Draw a menu list (main menu, steering, throttle, model select)
static void draw_menu_list(const menu_t *menu, const config_t *cfg) {
    char label[24];
    char value[16];
    uint8_t count = menu_item_count(menu);
    uint16_t y = 4;

    // Title bar
    const char *title = "MENU";
    switch (menu->state) {
        case MENU_STEERING:    title = "STEERING"; break;
        case MENU_THROTTLE:    title = "THROTTLE"; break;
        case MENU_MODEL_SELECT: title = "MODEL SELECT"; break;
        default: break;
    }
    display_fill_rect(0, 0, DISPLAY_WIDTH, 18, COLOR_BLUE);
    display_draw_string(4, 1, title, COLOR_WHITE, COLOR_BLUE);
    y = 22;

    for (uint8_t i = 0; i < count && y < DISPLAY_HEIGHT - 4; i++) {
        menu_item_text(menu, i, cfg, label, value);

        uint16_t bg = (i == menu->cursor) ? COLOR_BLUE : COLOR_BLACK;
        uint16_t fg = COLOR_WHITE;

        if (i == menu->cursor && menu->editing) {
            bg = COLOR_ORANGE;
            fg = COLOR_BLACK;
        }

        char line[42];
        if (value[0]) {
            snprintf(line, sizeof(line), " %-16s %s", label, value);
        } else {
            snprintf(line, sizeof(line), " %s", label);
        }
        draw_row(0, y + 1, line, fg, bg);
        y += 18;
    }
}

// Draw calibration wizard screen
static void draw_calibration(const calibrate_t *cal) {
    display_fill_rect(0, 0, DISPLAY_WIDTH, 18, COLOR_YELLOW);
    display_draw_string(4, 1, "CALIBRATION", COLOR_BLACK, COLOR_YELLOW);

    uint16_t y = 30;

    if (cal->instruction) {
        draw_row(8, y, cal->instruction, COLOR_WHITE, COLOR_BLACK);
    }

    y += 30;

    char buf[32];
    snprintf(buf, sizeof(buf), "ADC: %u", cal->current_adc);
    draw_row(8, y, buf, COLOR_CYAN, COLOR_BLACK);

    y += 20;

    if (cal->confirm_pending) {
        draw_row(8, y, "Click encoder to record", COLOR_GREEN, COLOR_BLACK);
    } else if (cal->sample_count > 0) {
        snprintf(buf, sizeof(buf), "Sampling: %u/64", cal->sample_count);
        draw_row(8, y, buf, COLOR_YELLOW, COLOR_BLACK);
    }

    y += 24;

    draw_row(8, y, "Long press = cancel", COLOR_GRAY, COLOR_BLACK);
}

// Main render function
void pages_render(const menu_t *menu, const shared_state_t *state,
                  const config_t *cfg, const calibrate_t *cal) {
    // No full-screen clear — each draw function paints its own background.
    // A full 320x172 fill takes ~30ms at 30MHz SPI, consuming the entire frame.
    static menu_state_t prev_state = MENU_DRIVING;
    static uint8_t prev_page = 255;

    // Only clear when switching pages/modes (to remove stale content)
    if (menu->state != prev_state || menu->display_page != prev_page) {
        display_fill(COLOR_BLACK);
        prev_state = menu->state;
        prev_page = menu->display_page;
    }

    switch (menu->state) {
        case MENU_DRIVING:
            switch (menu->display_page) {
                case 0:  draw_page_driving(state, cfg); break;
                case 1:  draw_page_big_numbers(state, cfg); break;
                case 2:  draw_page_debug(state, cfg); break;
                default: draw_page_driving(state, cfg); break;
            }
            break;

        case MENU_MAIN:
        case MENU_STEERING:
        case MENU_THROTTLE:
        case MENU_MODEL_SELECT:
            draw_menu_list(menu, cfg);
            break;

        case MENU_CALIBRATING:
            draw_calibration(cal);
            break;
    }
}
