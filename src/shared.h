#ifndef SHARED_H
#define SHARED_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    UI_MODE_BOOT = 0,
    UI_MODE_DRIVE,
    UI_MODE_WIFI,
} ui_mode_t;

// Inter-core shared state for the status LCD.
typedef struct {
    ui_mode_t ui_mode;
    uint16_t ch_values[5];     // CRSF channels: steer, throttle, switch, steer trim, throttle limiter
    uint16_t live_values[5];   // Mixed live inputs before arm gating
    int8_t   rssi;
    uint8_t  lq;
    int8_t   snr;
    uint16_t vbat_mv;
    bool     link_active;
    bool     armed;
    bool     vbat_warn;
    bool     crawl_mode;
    bool     boot_switch_raw_on;
    bool     boot_switch_debounced_on;
    uint8_t  speed_limit_pct;
    uint8_t  boot_switch_on_pct;
    uint8_t  wifi_countdown_s;
    uint16_t boot_switch_edges;
    uint32_t crsf_tx_count;
    uint32_t crsf_rx_count;
    uint16_t raw_adc[4];
    uint32_t uptime_seconds;
} shared_state_t;

void shared_init(void);
void shared_update(const shared_state_t *state);
void shared_snapshot(shared_state_t *state);

#endif
