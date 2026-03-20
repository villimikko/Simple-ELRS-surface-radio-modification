#ifndef CRSF_H
#define CRSF_H

#include <stdint.h>
#include <stdbool.h>

// CRSF protocol constants
#define CRSF_SYNC_BYTE       0xC8  // Standard CRSF sync (handset → TX module)
#define CRSF_SYNC_TELEM      0xC8  // TX module → handset (also 0xC8)
#define CRSF_BAUDRATE        420000

// Frame types
#define CRSF_FRAMETYPE_RC_CHANNELS  0x16
#define CRSF_FRAMETYPE_LINK_STATS   0x14
#define CRSF_FRAMETYPE_BATTERY      0x08
#define CRSF_FRAMETYPE_COMMAND      0x32

// CRSF addresses
#define CRSF_ADDRESS_TX_MODULE      0xEA
#define CRSF_ADDRESS_HANDSET        0xEE

// Commands
#define CRSF_COMMAND_MODEL_SELECT   0x05

// Channel value range
#define CRSF_CHANNEL_MIN     172
#define CRSF_CHANNEL_CENTER  992
#define CRSF_CHANNEL_MAX     1811

// RC channels frame: 16 channels × 11 bits = 22 bytes payload
#define CRSF_RC_CHANNELS_PAYLOAD_SIZE  22

// Max frame size: sync + len + type + payload + crc
#define CRSF_MAX_FRAME_SIZE  64

// Telemetry data from TX module
typedef struct {
    int8_t   rssi;
    uint8_t  lq;
    int8_t   snr;
    uint8_t  tx_power;
    uint8_t  rf_mode;
    uint16_t vbat_mv;       // Vehicle battery millivolts
    bool     link_active;
} crsf_telemetry_t;

// Initialize CRSF on UART0 (GP0=TX, GP1=RX)
void crsf_init(void);

// Build and send RC channels frame
// channels: array of 16 channel values (CRSF range 172-1811)
void crsf_send_rc_channels(const uint16_t channels[16]);

// Poll for incoming telemetry, returns true if new data received
bool crsf_poll_telemetry(crsf_telemetry_t *telem);

// Send model select command to TX module (for model match)
void crsf_send_model_id(uint8_t model_id);

#endif
