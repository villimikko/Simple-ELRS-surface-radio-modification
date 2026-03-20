#include "crsf.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include <string.h>

#include "hardware_config.h"

#define CRSF_UART      uart0
#define CRSF_TX_PIN    PIN_CRSF_TX
#define CRSF_RX_PIN    PIN_CRSF_RX

// CRC8 lookup table (polynomial 0xD5)
static const uint8_t crc8_lut[256] = {
    0x00, 0xD5, 0x7F, 0xAA, 0xFE, 0x2B, 0x81, 0x54,
    0x29, 0xFC, 0x56, 0x83, 0xD7, 0x02, 0xA8, 0x7D,
    0x52, 0x87, 0x2D, 0xF8, 0xAC, 0x79, 0xD3, 0x06,
    0x7B, 0xAE, 0x04, 0xD1, 0x85, 0x50, 0xFA, 0x2F,
    0xA4, 0x71, 0xDB, 0x0E, 0x5A, 0x8F, 0x25, 0xF0,
    0x8D, 0x58, 0xF2, 0x27, 0x73, 0xA6, 0x0C, 0xD9,
    0xF6, 0x23, 0x89, 0x5C, 0x08, 0xDD, 0x77, 0xA2,
    0xDF, 0x0A, 0xA0, 0x75, 0x21, 0xF4, 0x5E, 0x8B,
    0x9D, 0x48, 0xE2, 0x37, 0x63, 0xB6, 0x1C, 0xC9,
    0xB4, 0x61, 0xCB, 0x1E, 0x4A, 0x9F, 0x35, 0xE0,
    0xCF, 0x1A, 0xB0, 0x65, 0x31, 0xE4, 0x4E, 0x9B,
    0xE6, 0x33, 0x99, 0x4C, 0x18, 0xCD, 0x67, 0xB2,
    0x39, 0xEC, 0x46, 0x93, 0xC7, 0x12, 0xB8, 0x6D,
    0x10, 0xC5, 0x6F, 0xBA, 0xEE, 0x3B, 0x91, 0x44,
    0x6B, 0xBE, 0x14, 0xC1, 0x95, 0x40, 0xEA, 0x3F,
    0x42, 0x97, 0x3D, 0xE8, 0xBC, 0x69, 0xC3, 0x16,
    0xEF, 0x3A, 0x90, 0x45, 0x11, 0xC4, 0x6E, 0xBB,
    0xC6, 0x13, 0xB9, 0x6C, 0x38, 0xED, 0x47, 0x92,
    0xBD, 0x68, 0xC2, 0x17, 0x43, 0x96, 0x3C, 0xE9,
    0x94, 0x41, 0xEB, 0x3E, 0x6A, 0xBF, 0x15, 0xC0,
    0x4B, 0x9E, 0x34, 0xE1, 0xB5, 0x60, 0xCA, 0x1F,
    0x62, 0xB7, 0x1D, 0xC8, 0x9C, 0x49, 0xE3, 0x36,
    0x19, 0xCC, 0x66, 0xB3, 0xE7, 0x32, 0x98, 0x4D,
    0x30, 0xE5, 0x4F, 0x9A, 0xCE, 0x1B, 0xB1, 0x64,
    0x72, 0xA7, 0x0D, 0xD8, 0x8C, 0x59, 0xF3, 0x26,
    0x5B, 0x8E, 0x24, 0xF1, 0xA5, 0x70, 0xDA, 0x0F,
    0x20, 0xF5, 0x5F, 0x8A, 0xDE, 0x0B, 0xA1, 0x74,
    0x09, 0xDC, 0x76, 0xA3, 0xF7, 0x22, 0x88, 0x5D,
    0xD6, 0x03, 0xA9, 0x7C, 0x28, 0xFD, 0x57, 0x82,
    0xFF, 0x2A, 0x80, 0x55, 0x01, 0xD4, 0x7E, 0xAB,
    0x84, 0x51, 0xFB, 0x2E, 0x7A, 0xAF, 0x05, 0xD0,
    0xAD, 0x78, 0xD2, 0x07, 0x53, 0x86, 0x2C, 0xF9,
};

static uint8_t crc8(const uint8_t *data, uint8_t len) {
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; i++) {
        crc = crc8_lut[crc ^ data[i]];
    }
    return crc;
}

// Telemetry receive buffer
static uint8_t rx_buf[CRSF_MAX_FRAME_SIZE];
static uint8_t rx_pos = 0;

void crsf_init(void) {
    uart_init(CRSF_UART, CRSF_BAUDRATE);
    gpio_set_function(CRSF_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(CRSF_RX_PIN, GPIO_FUNC_UART);
    uart_set_format(CRSF_UART, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(CRSF_UART, true);
}

void crsf_send_rc_channels(const uint16_t channels[16]) {
    // Build frame: sync(1) + len(1) + type(1) + payload(22) + crc(1) = 26 bytes
    uint8_t frame[26];

    frame[0] = CRSF_SYNC_BYTE;
    frame[1] = 24;  // len = type(1) + payload(22) + crc(1)
    frame[2] = CRSF_FRAMETYPE_RC_CHANNELS;

    // Pack 16 channels × 11 bits into 22 bytes, little-endian
    uint8_t *payload = &frame[3];
    memset(payload, 0, CRSF_RC_CHANNELS_PAYLOAD_SIZE);

    uint32_t bit_offset = 0;
    for (int i = 0; i < 16; i++) {
        uint32_t val = channels[i] & 0x7FF;  // 11 bits
        uint32_t byte_idx = bit_offset / 8;
        uint32_t bit_idx = bit_offset % 8;

        // Spread across up to 3 bytes
        payload[byte_idx]     |= (val << bit_idx) & 0xFF;
        payload[byte_idx + 1] |= (val >> (8 - bit_idx)) & 0xFF;
        if (bit_idx > 5) {
            payload[byte_idx + 2] |= (val >> (16 - bit_idx)) & 0xFF;
        }

        bit_offset += 11;
    }

    // CRC over type + payload (bytes 2..24)
    frame[25] = crc8(&frame[2], 23);

    uart_write_blocking(CRSF_UART, frame, 26);
}

// CRC8 with polynomial 0xBA (reflected in/out) for command frames
static uint8_t crc8_ba(const uint8_t *data, uint8_t len) {
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x01)
                crc = (crc >> 1) ^ 0x5D;  // 0xBA reflected
            else
                crc >>= 1;
        }
    }
    return crc;
}

void crsf_send_model_id(uint8_t model_id) {
    // CRSF command frame: handset → TX module, model select
    // Format from elrs-joystick-control reference implementation
    uint8_t frame[10];
    frame[0] = CRSF_SYNC_TELEM;             // 0xC8 for command frames
    frame[1] = 8;                             // length: bytes 2-9
    frame[2] = CRSF_FRAMETYPE_COMMAND;        // 0x32
    frame[3] = CRSF_ADDRESS_HANDSET;          // 0xEE dest (TX module endpoint)
    frame[4] = CRSF_ADDRESS_TX_MODULE;        // 0xEA origin (handset endpoint)
    frame[5] = 0x10;                          // subcommand type
    frame[6] = CRSF_COMMAND_MODEL_SELECT;     // 0x05
    frame[7] = model_id;
    frame[8] = crc8_ba(&frame[2], 6);         // inner CRC (0xBA) over bytes 2-7
    frame[9] = crc8(&frame[2], 7);            // outer CRC (0xD5) over bytes 2-8

    uart_write_blocking(CRSF_UART, frame, 10);
}

bool crsf_poll_telemetry(crsf_telemetry_t *telem) {
    bool got_new = false;

    while (uart_is_readable(CRSF_UART)) {
        uint8_t byte = uart_getc(CRSF_UART);

        if (rx_pos == 0 && byte != 0xC8 && byte != 0xEA && byte != 0xEE) {
            continue;  // Wait for sync (TX module can use 0xC8, 0xEA, or 0xEE)
        }

        rx_buf[rx_pos++] = byte;

        // Need at least 2 bytes to know frame length
        if (rx_pos < 2) continue;

        uint8_t frame_len = rx_buf[1];  // Bytes after len field
        uint8_t total_len = frame_len + 2;  // sync + len + rest

        if (total_len > CRSF_MAX_FRAME_SIZE) {
            rx_pos = 0;
            continue;
        }

        if (rx_pos < total_len) continue;

        // Full frame received — verify CRC
        uint8_t calc_crc = crc8(&rx_buf[2], frame_len - 1);
        uint8_t recv_crc = rx_buf[total_len - 1];

        if (calc_crc == recv_crc) {
            uint8_t type = rx_buf[2];

            if (type == CRSF_FRAMETYPE_LINK_STATS && frame_len >= 11) {
                telem->rssi     = (int8_t)rx_buf[3];
                telem->lq       = rx_buf[5];
                telem->snr      = (int8_t)rx_buf[6];
                telem->rf_mode  = rx_buf[8];
                telem->tx_power = rx_buf[7];
                telem->link_active = true;
                got_new = true;
            }
            else if (type == CRSF_FRAMETYPE_BATTERY && frame_len >= 6) {
                // Voltage is big-endian, in 100mV units
                uint16_t vbat_raw = ((uint16_t)rx_buf[3] << 8) | rx_buf[4];
                telem->vbat_mv = vbat_raw * 100;
                got_new = true;
            }
        }

        rx_pos = 0;
    }

    return got_new;
}
