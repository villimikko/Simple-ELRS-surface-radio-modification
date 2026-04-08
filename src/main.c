// ELRS Surface Radio — KISS Firmware
// RP2350 (Waveshare RP2350-LCD-1.47-A)
//
// Boot → Drive. No menus, no calibration, no encoder-driven UI.
// Hardcoded pot calibration. Steering trim offsets CH1, throttle trim is a speed limiter.
// Switch = arm/disarm. LED = link status. LCD = status page.
//
// Core 0 handles controls/CRSF. Core 1 renders the LCD status page.
// USB CDC for serial debug.

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
#include "screen.h"
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
#define STEER_TRIM_LIMIT    0.20f   // trim knob can shift steering by +/-20% of full throw

// Hardcoded calibration
static const cal_pot_t cal_steer      = CAL_STEERING;
static const cal_pot_t cal_throttle   = CAL_THROTTLE;
static const cal_pot_t cal_trim_steer = CAL_TRIM_STEER;
static const cal_pot_t cal_trim_throt = CAL_TRIM_THROT;

// What was actually sent over CRSF (for serial debug)
static uint16_t g_sent[5] = {992, 992, 172, 992, 992};

#define WIFI_TIMEOUT_MS      60000
#define SWITCH_SAMPLE_MS     10
#define SWITCH_DEBOUNCE_MS   80
#define SWITCH_LOG_MS        1000

typedef struct {
    bool raw_on;
    bool debounced_on;
    uint16_t raw_edges;
    uint32_t on_samples;
    uint32_t total_samples;
    uint64_t raw_changed_us;
    uint64_t last_log_us;
} boot_switch_state_t;

static float trim_channel_to_unit(uint16_t trim_channel) {
    float trim = (float)((int)trim_channel - CRSF_CHANNEL_CENTER) /
                 (float)(CRSF_CHANNEL_MAX - CRSF_CHANNEL_CENTER);

    if (trim < -1.0f) trim = -1.0f;
    if (trim >  1.0f) trim =  1.0f;

    return trim;
}

static uint16_t apply_steer_trim(uint16_t steer_channel, uint16_t trim_channel) {
    float trim = trim_channel_to_unit(trim_channel);
    int trim_delta = (int)(trim * STEER_TRIM_LIMIT *
                           (float)(CRSF_CHANNEL_MAX - CRSF_CHANNEL_CENTER));
    int steer = (int)steer_channel + trim_delta;

    if (steer < CRSF_CHANNEL_MIN) steer = CRSF_CHANNEL_MIN;
    if (steer > CRSF_CHANNEL_MAX) steer = CRSF_CHANNEL_MAX;

    return (uint16_t)steer;
}

static float throttle_speed_limit(uint16_t trim_channel) {
    float speed_limit = (float)(trim_channel - CRSF_CHANNEL_MIN) /
                        (float)(CRSF_CHANNEL_MAX - CRSF_CHANNEL_MIN);

    if (speed_limit < 0.0f) speed_limit = 0.0f;
    if (speed_limit > 1.0f) speed_limit = 1.0f;

    return speed_limit;
}

static void arm_switch_boot_init(void) {
    gpio_init(PIN_SWITCH1);
    gpio_set_dir(PIN_SWITCH1, GPIO_IN);
    gpio_pull_up(PIN_SWITCH1);
    sleep_ms(20);
}

static void boot_switch_state_init(boot_switch_state_t *sw, uint64_t now_us) {
    memset(sw, 0, sizeof(*sw));
    sw->raw_on = !gpio_get(PIN_SWITCH1);
    sw->debounced_on = sw->raw_on;
    sw->raw_changed_us = now_us;
    sw->last_log_us = now_us;
}

static bool boot_switch_update(boot_switch_state_t *sw, shared_state_t *ui, uint64_t now_us) {
    bool raw_on = !gpio_get(PIN_SWITCH1);

    sw->total_samples++;
    if (raw_on) sw->on_samples++;

    if (raw_on != sw->raw_on) {
        sw->raw_on = raw_on;
        sw->raw_changed_us = now_us;
        sw->raw_edges++;
    }

    if (sw->debounced_on != sw->raw_on &&
        now_us - sw->raw_changed_us >= (uint64_t)SWITCH_DEBOUNCE_MS * 1000) {
        sw->debounced_on = sw->raw_on;
    }

    ui->boot_switch_raw_on = sw->raw_on;
    ui->boot_switch_debounced_on = sw->debounced_on;
    ui->boot_switch_edges = sw->raw_edges;
    ui->boot_switch_on_pct = (sw->total_samples == 0)
        ? 0
        : (uint8_t)((sw->on_samples * 100u) / sw->total_samples);

    return sw->debounced_on;
}

static bool wait_for_arm_or_wifi(shared_state_t *ui) {
    uint64_t wait_start_us = time_us_64();
    uint64_t next_blue_toggle_us = wait_start_us;
    boot_switch_state_t sw;
    bool blue_on = false;

    arm_switch_boot_init();
    boot_switch_state_init(&sw, wait_start_us);

    while (true) {
        uint64_t now_us = time_us_64();
        uint32_t elapsed_ms = (uint32_t)((now_us - wait_start_us) / 1000);

        if (ui->ui_mode == UI_MODE_BOOT) {
            uint32_t remaining_ms = (elapsed_ms >= WIFI_TIMEOUT_MS)
                                  ? 0
                                  : (WIFI_TIMEOUT_MS - elapsed_ms);
            ui->wifi_countdown_s = (uint8_t)((remaining_ms + 999) / 1000);
        } else {
            ui->wifi_countdown_s = 0;
        }

        if (boot_switch_update(&sw, ui, now_us)) {
            printf("ARM switch ON -> starting CRSF\n");
            ui->armed = true;
            shared_update(ui);
            led_starting();
            return true;
        }

        if (now_us - sw.last_log_us >= (uint64_t)SWITCH_LOG_MS * 1000) {
            sw.last_log_us += (uint64_t)SWITCH_LOG_MS * 1000;
            printf("$S,mode=%u,raw=%u,db=%u,edges=%u,on=%u,wifi=%u\n",
                   ui->ui_mode,
                   ui->boot_switch_raw_on ? 1 : 0,
                   ui->boot_switch_debounced_on ? 1 : 0,
                   ui->boot_switch_edges,
                   ui->boot_switch_on_pct,
                   ui->wifi_countdown_s);
        }

        if (ui->ui_mode == UI_MODE_BOOT && elapsed_ms >= WIFI_TIMEOUT_MS) {
            ui->ui_mode = UI_MODE_WIFI;
            ui->wifi_countdown_s = 0;
            printf("WiFi standby after %dms with ARM switch OFF\n", WIFI_TIMEOUT_MS);
        }

        if (ui->ui_mode == UI_MODE_WIFI) {
            if (now_us >= next_blue_toggle_us) {
                next_blue_toggle_us = now_us + 500000;
                blue_on = !blue_on;
                if (blue_on) {
                    led_set_rgb(0, 0, 20);
                } else {
                    led_set_rgb(0, 0, 0);
                }
            }
        }

        shared_update(ui);
        tud_task();
        sleep_ms(SWITCH_SAMPLE_MS);
    }
}

int main(void) {
    shared_state_t ui = {0};
    ui.ui_mode = UI_MODE_BOOT;

    stdio_init_all();
    shared_init();
    shared_update(&ui);
    screen_start();

    sleep_ms(3000);
    printf("ELRS Surface Radio — KISS\n");

    led_init();
    led_starting();  // orange = booting

    printf("Boot screen: ARM ON starts CRSF, ARM OFF enters WiFi after 60s\n");
    wait_for_arm_or_wifi(&ui);

    adc_inputs_init();
    crsf_init();
    buttons_init();
    ui.ui_mode = UI_MODE_DRIVE;
    ui.boot_switch_raw_on = false;
    ui.boot_switch_debounced_on = false;
    ui.boot_switch_on_pct = 0;
    ui.boot_switch_edges = 0;
    ui.wifi_countdown_s = 0;
    shared_update(&ui);

    printf("Ready.\n");

    uint64_t next_crsf = time_us_64();
    uint64_t next_serial = time_us_64();
    uint64_t next_led = time_us_64();
    uint64_t next_model_id = time_us_64();  // Send model ID immediately at boot
    uint32_t uptime = 0;
    uint64_t next_uptime = time_us_64() + 1000000;
    uint32_t crsf_tx = 0;
    uint32_t crsf_rx = 0;

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
            crsf_rx++;
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
            uint16_t steer_input = mixer_process(adc_steer, &cal_steer, true);
            uint16_t throt_input = mixer_process(adc_throt, &cal_throttle, true);
            uint16_t ch_ts       = mixer_process(adc_ts, &cal_trim_steer, false);
            uint16_t ch_steer    = apply_steer_trim(steer_input, ch_ts);

            // Throttle trim = max speed limiter (0.0 to 1.0)
            uint16_t trim_raw = mixer_process(adc_tt, &cal_trim_throt, false);
            float speed_limit = throttle_speed_limit(trim_raw);

            // Battery crawl mode overrides speed limit
            bool crawl_mode = (telem.vbat_mv > 0 && telem.vbat_mv < VBAT_CRAWL_MV);
            if (crawl_mode && speed_limit > CRAWL_SPEED_LIMIT)
                speed_limit = CRAWL_SPEED_LIMIT;
            bool vbat_warn = (telem.vbat_mv > 0 && telem.vbat_mv < VBAT_WARNING_MV);

            // Apply speed limit: scale throttle deviation from center
            uint16_t ch_throt = throt_input;
            float throt_dev = (float)((int)throt_input - CRSF_CHANNEL_CENTER);
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

            ui.ch_values[0] = channels[0];
            ui.ch_values[1] = channels[1];
            ui.ch_values[2] = channels[2];
            ui.ch_values[3] = channels[3];
            ui.ch_values[4] = channels[4];
            ui.live_values[0] = steer_input;
            ui.live_values[1] = throt_input;
            ui.live_values[2] = ch_sw;
            ui.live_values[3] = ch_ts;
            ui.live_values[4] = trim_raw;
            ui.raw_adc[0] = adc_steer;
            ui.raw_adc[1] = adc_throt;
            ui.raw_adc[2] = adc_ts;
            ui.raw_adc[3] = adc_tt;
            ui.armed = armed;
            ui.speed_limit_pct = (uint8_t)(speed_limit * 100.0f + 0.5f);
            ui.crawl_mode = crawl_mode;
            ui.vbat_warn = vbat_warn;
            ui.rssi = telem.rssi;
            ui.lq = telem.lq;
            ui.snr = telem.snr;
            ui.vbat_mv = telem.vbat_mv;
            ui.link_active = link_up;
            ui.crsf_tx_count = crsf_tx;
            ui.crsf_rx_count = crsf_rx;
            ui.uptime_seconds = uptime;
            shared_update(&ui);
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

            uint16_t ch_s_raw = mixer_process(adc[0], &cal_steer, true);
            uint16_t ch_t_raw = mixer_process(adc[1], &cal_throttle, true);
            bool armed = switch1_read();
            uint16_t ch_ts = mixer_process(adc[2], &cal_trim_steer, false);
            uint16_t ch_tt = mixer_process(adc[3], &cal_trim_throt, false);
            uint16_t ch_s = apply_steer_trim(ch_s_raw, ch_ts);
            float speed_limit = throttle_speed_limit(ch_tt);
            uint16_t ch_t = ch_t_raw;
            float throt_dev = (float)((int)ch_t_raw - CRSF_CHANNEL_CENTER);
            throt_dev *= speed_limit;
            ch_t = (uint16_t)(CRSF_CHANNEL_CENTER + throt_dev);
            if (ch_t < CRSF_CHANNEL_MIN) ch_t = CRSF_CHANNEL_MIN;
            if (ch_t > CRSF_CHANNEL_MAX) ch_t = CRSF_CHANNEL_MAX;

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
