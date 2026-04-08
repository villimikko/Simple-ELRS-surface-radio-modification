#include "screen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/multicore.h"
#include "pico/stdlib.h"

#include "crsf.h"
#include "shared.h"
#include "unused/display.h"

#define SCREEN_REFRESH_MS  200
#define SCREEN_MARGIN_X    4
#define SCREEN_BAR_X       76
#define SCREEN_BAR_W       236
#define SCREEN_BAR_H       12

static bool g_screen_started = false;

static int channel_to_pct(uint16_t value) {
    int pct = ((int)value - CRSF_CHANNEL_CENTER) * 100
              / (CRSF_CHANNEL_MAX - CRSF_CHANNEL_CENTER);
    if (pct < -100) pct = -100;
    if (pct > 100) pct = 100;
    return pct;
}

static void draw_text_row(uint16_t y, const char *text, uint16_t fg, uint16_t bg) {
    display_fill_rect(0, y, DISPLAY_WIDTH, FONT_HEIGHT, bg);
    display_draw_string(SCREEN_MARGIN_X, y, text, fg, bg);
}

static void draw_header(const char *status, uint16_t status_color) {
    display_fill_rect(0, 0, DISPLAY_WIDTH, 18, COLOR_DARK_GRAY);
    display_draw_string(4, 1, "ELRS SURFACE", COLOR_WHITE, COLOR_DARK_GRAY);
    display_draw_string(248, 1, status, status_color, COLOR_DARK_GRAY);
}

static void draw_center_bar(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                            int pct, uint16_t fg) {
    if (pct < -100) pct = -100;
    if (pct > 100) pct = 100;

    display_fill_rect(x, y, w, h, COLOR_DARK_GRAY);

    uint16_t center_x = x + (w / 2);
    display_fill_rect(center_x - 1, y, 2, h, COLOR_WHITE);

    uint16_t half_w = w / 2;
    uint16_t fill_w = (uint16_t)((abs(pct) * half_w) / 100);
    if (fill_w == 0) return;

    if (pct > 0) {
        display_fill_rect(center_x, y, fill_w, h, fg);
    } else {
        display_fill_rect(center_x - fill_w, y, fill_w, h, fg);
    }
}

static void render_boot(const shared_state_t *state) {
    char line[48];

    draw_header("BOOT", COLOR_ORANGE);
    draw_text_row(32, "ARM ON starts CRSF", COLOR_WHITE, COLOR_BLACK);
    snprintf(line, sizeof(line), "RAW %s  DB %s",
             state->boot_switch_raw_on ? "ON " : "OFF",
             state->boot_switch_debounced_on ? "ON " : "OFF");
    draw_text_row(56, line,
                  state->boot_switch_debounced_on ? COLOR_GREEN : COLOR_YELLOW,
                  COLOR_BLACK);
    snprintf(line, sizeof(line), "ON %3u%%  EDGES %u",
             state->boot_switch_on_pct, state->boot_switch_edges);
    draw_text_row(80, line, COLOR_CYAN, COLOR_BLACK);
    snprintf(line, sizeof(line), "ARM OFF -> WiFi in %2us",
             state->wifi_countdown_s);
    draw_text_row(104, line, COLOR_GRAY, COLOR_BLACK);
    draw_text_row(128, "Flip ARM ON to transmit", COLOR_WHITE, COLOR_BLACK);
    display_fill_rect(24, 132, 80, 18, COLOR_RED);
    display_fill_rect(120, 132, 80, 18, COLOR_GREEN);
    display_fill_rect(216, 132, 80, 18, COLOR_BLUE);
}

static void render_wifi(const shared_state_t *state) {
    char line[48];

    draw_header("WIFI", COLOR_BLUE);
    draw_text_row(32, "TX module WiFi standby", COLOR_WHITE, COLOR_BLACK);
    snprintf(line, sizeof(line), "RAW %s  DB %s",
             state->boot_switch_raw_on ? "ON " : "OFF",
             state->boot_switch_debounced_on ? "ON " : "OFF");
    draw_text_row(56, line,
                  state->boot_switch_debounced_on ? COLOR_GREEN : COLOR_YELLOW,
                  COLOR_BLACK);
    snprintf(line, sizeof(line), "ON %3u%%  EDGES %u",
             state->boot_switch_on_pct, state->boot_switch_edges);
    draw_text_row(80, line, COLOR_CYAN, COLOR_BLACK);
    snprintf(line, sizeof(line), "ARM %-3s to start CRSF",
             state->boot_switch_debounced_on ? "ON" : "OFF");
    draw_text_row(104, line, COLOR_GRAY, COLOR_BLACK);
    draw_text_row(128, "CRSF idle after 60s timeout", COLOR_WHITE, COLOR_BLACK);
}

static void render_drive(const shared_state_t *state) {
    char line[48];
    int steer_pct = channel_to_pct(state->live_values[0]);
    int throt_pct = channel_to_pct(state->live_values[1]);

    draw_header(state->link_active ? "LINK" : "NO LINK",
                state->link_active ? COLOR_GREEN : COLOR_RED);

    snprintf(line, sizeof(line), "ARM %-3s  SPD %3u%%  %s",
             state->armed ? "ON" : "OFF",
             state->speed_limit_pct,
             state->crawl_mode ? "CRAWL" : (state->vbat_warn ? "LOW BAT" : "READY"));
    draw_text_row(22, line, COLOR_WHITE, COLOR_BLACK);

    snprintf(line, sizeof(line), "STEER %+4d%%  IN %4u TX %4u",
             steer_pct, state->live_values[0], state->ch_values[0]);
    draw_text_row(46, line, COLOR_WHITE, COLOR_BLACK);
    draw_center_bar(SCREEN_BAR_X, 64, SCREEN_BAR_W, SCREEN_BAR_H, steer_pct, COLOR_CYAN);

    snprintf(line, sizeof(line), "THROT %+4d%%  IN %4u TX %4u",
             throt_pct, state->live_values[1], state->ch_values[1]);
    draw_text_row(84, line, COLOR_WHITE, COLOR_BLACK);
    draw_center_bar(SCREEN_BAR_X, 102, SCREEN_BAR_W, SCREEN_BAR_H, throt_pct,
                    state->crawl_mode ? COLOR_ORANGE : COLOR_GREEN);

    if (!state->armed) {
        snprintf(line, sizeof(line), "ARM OFF -> TX steer/throt centered");
    } else if (state->link_active) {
        snprintf(line, sizeof(line), "RSSI %4d  LQ %3u  SNR %3d",
                 state->rssi, state->lq, state->snr);
    } else {
        snprintf(line, sizeof(line), "Waiting for receiver telemetry");
    }
    draw_text_row(120, line, state->link_active ? COLOR_WHITE : COLOR_RED, COLOR_BLACK);

    if (state->vbat_mv > 0) {
        snprintf(line, sizeof(line), "VBAT %u.%uV  TX#%lu",
                 state->vbat_mv / 1000,
                 (state->vbat_mv % 1000) / 100,
                 state->crsf_tx_count);
    } else {
        snprintf(line, sizeof(line), "VBAT --  TX#%lu", state->crsf_tx_count);
    }
    draw_text_row(138, line, COLOR_YELLOW, COLOR_BLACK);

    uint32_t s = state->uptime_seconds;
    snprintf(line, sizeof(line), "UP %02lu:%02lu:%02lu  RX#%lu",
             s / 3600,
             (s / 60) % 60,
             s % 60,
             state->crsf_rx_count);
    draw_text_row(156, line, COLOR_GRAY, COLOR_BLACK);
}

static void screen_core1_main(void) {
    ui_mode_t prev_mode = (ui_mode_t)255;
    shared_state_t state;

    display_init();

    while (true) {
        shared_snapshot(&state);

        if (state.ui_mode != prev_mode) {
            display_fill(COLOR_BLACK);
            prev_mode = state.ui_mode;
        }

        switch (state.ui_mode) {
            case UI_MODE_WIFI:
                render_wifi(&state);
                break;
            case UI_MODE_DRIVE:
                render_drive(&state);
                break;
            case UI_MODE_BOOT:
            default:
                render_boot(&state);
                break;
        }

        sleep_ms(SCREEN_REFRESH_MS);
    }
}

void screen_start(void) {
    if (g_screen_started) return;
    g_screen_started = true;
    multicore_launch_core1(screen_core1_main);
}
