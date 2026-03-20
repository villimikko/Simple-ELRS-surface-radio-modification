// ELRS Surface Radio — KISS Firmware
// RP2350 (Waveshare RP2350-LCD-1.47-A)
//
// Boot → Drive. No menus, no calibration, no encoder, no display.
// Hardcoded pot calibration. Throttle trim = max speed limiter.
// Switch = arm/disarm. LED = link status.
//
// Core 0 only (single core). USB CDC for serial debug.

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/gpio.h"
#include "tusb.h"

#include "crsf.h"
#include "adc.h"
#include "led.h"
#include "config.h"
#include "mixer.h"
#include "shared.h"
#include "buttons.h"
#include "hardware_config.h"

// Timing
#define CRSF_INTERVAL_US    4000    // 250Hz
#define SERIAL_INTERVAL_US  100000  // 10Hz debug output
#define LED_INTERVAL_US     200000  // 5Hz LED update

// Link timeout: if no telemetry for 2 seconds, consider link lost
#define LINK_TIMEOUT_US     2000000

// 2S LiPo battery thresholds (millivolts, from receiver telemetry)
#define VBAT_WARNING_MV     7000    // 3.5V/cell — purple flash warning
#define VBAT_CRAWL_MV       6000    // 3.0V/cell — crawl mode (10% throttle)
#define CRAWL_SPEED_LIMIT   0.10f   // max throttle in crawl mode

// Hardcoded calibration
static const cal_pot_t cal_steer      = CAL_STEERING;
static const cal_pot_t cal_throttle   = CAL_THROTTLE;
static const cal_pot_t cal_trim_steer = CAL_TRIM_STEER;
static const cal_pot_t cal_trim_throt = CAL_TRIM_THROT;

// What was actually sent over CRSF (for serial debug)
static uint16_t g_sent[5] = {992, 992, 172, 992, 992};

int main(void) {
    stdio_init_all();
    sleep_ms(3000);
    printf("ELRS Surface Radio — KISS\n");

    led_init();
    led_starting();  // orange = booting

    // Check encoder button (GP4) at boot: held = WiFi mode (no CRSF)
    // Sample multiple times to avoid false trigger from picotool reboot glitch
    gpio_init(PIN_ENC_SW);
    gpio_set_dir(PIN_ENC_SW, GPIO_IN);
    gpio_pull_up(PIN_ENC_SW);
    sleep_ms(50);  // pull-up settle

    int btn_low = 0;
    for (int i = 0; i < 5; i++) {
        if (!gpio_get(PIN_ENC_SW)) btn_low++;
        sleep_ms(10);
    }
    printf("GP4 check: %d/5 low\n", btn_low);

    if (btn_low >= 4) {
        // Button held at boot → WiFi mode
        // Never start CRSF so TX module enters WiFi after ~60s
        // Wait 30s before showing blue — avoids false visual on glitchy boot
        printf("WiFi mode — CRSF disabled, waiting 30s...\n");
        for (int i = 0; i < 60; i++) {
            sleep_ms(500);
            tud_task();
        }
        printf("TX should enter WiFi soon\n");
        uint8_t blink = 0;
        while (true) {
            blink++;
            if (blink & 1)
                led_set_rgb(0, 0, 20);   // blue
            else
                led_set_rgb(0, 0, 0);    // off
            sleep_ms(500);
            tud_task();
        }
    }

    shared_init();
    adc_inputs_init();
    crsf_init();
    buttons_init();

    printf("Ready.\n");

    uint64_t next_crsf = time_us_64();
    uint64_t next_serial = time_us_64();
    uint64_t next_led = time_us_64();
    uint64_t next_model_id = time_us_64();  // Send model ID immediately at boot
    uint32_t uptime = 0;
    uint64_t next_uptime = time_us_64() + 1000000;
    uint32_t crsf_tx = 0;

    // Telemetry state
    crsf_telemetry_t telem = {0};
    bool ever_got_telem = false;
    uint64_t last_telem_time = 0;
    bool link_up = false;
    uint8_t led_blink = 0;

    while (true) {
        uint64_t now = time_us_64();

        // Poll CRSF telemetry (every loop iteration — fast)
        if (crsf_poll_telemetry(&telem)) {
            last_telem_time = now;
            ever_got_telem = true;
        }

        // Link status: need recent telemetry WITH actual link quality > 0
        // TX module sends telemetry even without receiver (lq=0)
        link_up = ever_got_telem && telem.lq > 0 && (now - last_telem_time < LINK_TIMEOUT_US);

        // 250Hz CRSF loop
        if (now >= next_crsf) {
            next_crsf += CRSF_INTERVAL_US;

            adc_read_all();

            uint16_t adc_steer = adc_get(ADC_CH_STEERING);
            uint16_t adc_throt = adc_get(ADC_CH_THROTTLE);
            uint16_t adc_ts    = adc_get(ADC_CH_TRIM_STEER);
            uint16_t adc_tt    = adc_get(ADC_CH_TRIM_THROT);

            // Process main channels
            uint16_t ch_steer = mixer_process(adc_steer, &cal_steer, true);
            uint16_t ch_throt = mixer_process(adc_throt, &cal_throttle, true);
            uint16_t ch_ts    = mixer_process(adc_ts, &cal_trim_steer, false);

            // Throttle trim = max speed limiter (0.0 to 1.0)
            uint16_t trim_raw = mixer_process(adc_tt, &cal_trim_throt, false);
            float speed_limit = (float)(trim_raw - CRSF_CHANNEL_MIN) /
                               (float)(CRSF_CHANNEL_MAX - CRSF_CHANNEL_MIN);
            if (speed_limit < 0.0f) speed_limit = 0.0f;
            if (speed_limit > 1.0f) speed_limit = 1.0f;

            // Battery crawl mode overrides speed limit
            bool crawl_mode = (telem.vbat_mv > 0 && telem.vbat_mv < VBAT_CRAWL_MV);
            if (crawl_mode && speed_limit > CRAWL_SPEED_LIMIT)
                speed_limit = CRAWL_SPEED_LIMIT;

            // Apply speed limit: scale throttle deviation from center
            float throt_dev = (float)((int)ch_throt - CRSF_CHANNEL_CENTER);
            throt_dev *= speed_limit;
            ch_throt = (uint16_t)(CRSF_CHANNEL_CENTER + throt_dev);
            if (ch_throt < CRSF_CHANNEL_MIN) ch_throt = CRSF_CHANNEL_MIN;
            if (ch_throt > CRSF_CHANNEL_MAX) ch_throt = CRSF_CHANNEL_MAX;

            // Arm switch
            bool armed = switch1_read();
            uint16_t ch_sw = armed ? CRSF_CHANNEL_MAX : CRSF_CHANNEL_MIN;

            // Build CRSF packet
            // Disarmed: all channels center (1500 PWM), only arm state sent
            uint16_t channels[16];
            for (int i = 0; i < 16; i++) channels[i] = CRSF_CHANNEL_CENTER;

            if (armed) {
                channels[0] = ch_steer;
                channels[1] = ch_throt;
                channels[3] = ch_ts;
                channels[4] = trim_raw;
            }
            channels[2] = ch_sw;

            crsf_send_rc_channels(channels);
            crsf_tx++;

            // Store what was actually sent over CRSF
            g_sent[0] = channels[0];
            g_sent[1] = channels[1];
            g_sent[2] = channels[2];
            g_sent[3] = channels[3];
            g_sent[4] = channels[4];
        }

        // 5Hz LED update
        if (now >= next_led) {
            next_led += LED_INTERVAL_US;
            led_blink++;

            bool vbat_warn = (telem.vbat_mv > 0 && telem.vbat_mv < VBAT_WARNING_MV);
            bool vbat_crawl = (telem.vbat_mv > 0 && telem.vbat_mv < VBAT_CRAWL_MV);

            if (link_up) {
                if (vbat_crawl) {
                    // Crawl mode: fast purple blink
                    if (led_blink & 1)
                        led_set_rgb(20, 0, 20);  // purple
                    else
                        led_set_rgb(0, 0, 0);
                } else if (vbat_warn) {
                    // Low battery: alternate green/purple
                    if (led_blink & 1)
                        led_set_rgb(0, 20, 0);   // green
                    else
                        led_set_rgb(20, 0, 20);  // purple
                } else {
                    // Connected: solid green
                    led_set_rgb(0, 20, 0);
                }
            } else {
                // No link: blink red
                if (led_blink & 1) {
                    led_set_rgb(20, 0, 0);
                } else {
                    led_set_rgb(0, 0, 0);
                }
            }
        }

        // Send model ID at boot then every 30 seconds
        if (now >= next_model_id) {
            next_model_id += 30000000;  // every 30 seconds
            crsf_send_model_id(0);      // model ID 0
        }

        // 1Hz uptime
        if (now >= next_uptime) {
            next_uptime += 1000000;
            uptime++;
        }

        // 10Hz serial debug
        // $D,sent0-4,mixer0-4,rssi,lq,link,vbat,uptime,tx
        if (now >= next_serial) {
            next_serial += SERIAL_INTERVAL_US;

            uint16_t adc[4] = {
                adc_get(ADC_CH_STEERING),
                adc_get(ADC_CH_THROTTLE),
                adc_get(ADC_CH_TRIM_STEER),
                adc_get(ADC_CH_TRIM_THROT),
            };

            uint16_t ch_s = mixer_process(adc[0], &cal_steer, true);
            uint16_t ch_t = mixer_process(adc[1], &cal_throttle, true);
            bool armed = switch1_read();
            uint16_t ch_ts = mixer_process(adc[2], &cal_trim_steer, false);
            uint16_t ch_tt = mixer_process(adc[3], &cal_trim_throt, false);

            printf("$D,%u,%u,%u,%u,%u,"
                   "%u,%u,%u,%u,%u,"
                   "%d,%u,%u,%u,"
                   "%lu,%lu\n",
                   // CRSF sent (what receiver gets)
                   g_sent[0], g_sent[1], g_sent[2], g_sent[3], g_sent[4],
                   // Mixer output (before arm gate)
                   ch_s, ch_t, armed ? 1 : 0, ch_ts, ch_tt,
                   // Telemetry
                   telem.rssi, telem.lq, link_up ? 1 : 0, telem.vbat_mv,
                   // Stats
                   uptime, crsf_tx);
        }

        tud_task();
    }

    return 0;
}
