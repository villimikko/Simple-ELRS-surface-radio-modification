#ifndef SHARED_H
#define SHARED_H

#include <stdint.h>
#include <stdbool.h>
#include "hardware/sync.h"

// Inter-core shared state.
// Core 0 writes channel/telemetry data (no lock — display tearing is harmless).
// Core 1 reads for display/serial output (uses lock for memcpy snapshot).
typedef struct {
    // Core 0 → Core 1
    uint16_t ch_values[5];     // CRSF channels: steer, throttle, switch, trim_s, trim_t
    int8_t   rssi;
    uint8_t  lq;
    int8_t   snr;
    uint16_t vbat_mv;
    bool     link_active;
    uint32_t crsf_tx_count;
    uint32_t crsf_rx_count;
    uint16_t raw_adc[4];

    // INA219
    uint16_t tx_voltage_mv;
    int16_t  tx_current_ma;
    uint16_t tx_power_mw;
    bool     ina219_present;

    // Uptime
    uint32_t uptime_seconds;
} shared_state_t;

extern shared_state_t g_shared;
extern spin_lock_t *g_shared_lock;

void shared_init(void);

// Lock/unlock — only used by Core 1 for snapshot memcpy
static inline void shared_lock(void) {
    spin_lock_blocking(g_shared_lock);
}

static inline void shared_unlock(void) {
    spin_unlock_unsafe(g_shared_lock);
}

#endif
