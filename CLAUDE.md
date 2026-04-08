# CLAUDE.md — ELRS Surface Radio Firmware

## Project Overview

DIY 2.4GHz ELRS surface radio controller for RC cars. Runs on **Waveshare RP2350-LCD-1.47-A** board with onboard 1.47" ST7789 LCD and WS2812 RGB LED. Communicates with SpeedyBee Nano RX (flashed as TX module) over CRSF protocol to RadioMaster ER5A V2 receiver.

## Build

```bash
cmake -S . -B build
cmake --build build -j$(nproc)

# Flash from a running board over USB serial:
picotool reboot -f -u
picotool load -v -x build/elrs-surface-radio.uf2
```

SDK: Pico SDK 2.2.0, Board: pico2 (RP2350)

## Architecture

- **Core 0**: Real-time CRSF loop at 250Hz — ADC read, hardcoded calibration, trim/limit handling, CRSF TX/RX, LED update
- **Core 1**: LCD status renderer — boot page, WiFi standby page, drive/telemetry page
- **Inter-core**: Shared state protected by spin lock (`shared.c`)

Active boot flow:
- `ARM ON` at power-up starts CRSF immediately
- `ARM OFF` stays in boot and falls into WiFi standby after 60 seconds
- No menu system, encoder-driven UI, or calibration wizard is active in the current firmware

## Source Files (src/)

| File | Purpose |
|------|---------|
| main.c | Dual-core entry, init sequence, main loops |
| crsf.c/h | CRSF protocol: 420kbaud UART, RC channels TX, telemetry RX |
| adc.c/h | 4-channel ADC with 16x oversampling + EMA smoothing |
| mixer.c/h | Pot calibration to CRSF range with deadband for spring-return controls |
| screen.c/h | Active LCD renderer for boot, WiFi, and drive status pages |
| unused/display.c/h | ST7789 LCD driver: SPI0 at 30MHz, landscape 320x172, 8x16 bitmap font |
| buttons.c/h | Toggle switch on GP5 (debounced, maps to CRSF CH3) |
| led.c/h | WS2812 via PIO (ws2812.pio) — red=no link, green=linked, orange=starting |
| shared.c/h | Inter-core shared state with spin lock |
| hardware_config.h | Central pin assignment and CRSF channel mapping config |
| ws2812.pio | PIO program for WS2812 LED (4 instructions) |

Inactive legacy modules are parked under `src/unused/`:
- menu system
- calibration wizard
- config/profile storage
- INA219 support
- old page renderer and encoder-driven UI

## Pin Assignments (hardware_config.h)

| GPIO | Function | Notes |
|------|----------|-------|
| GP0/GP1 | CRSF UART TX/RX | 420kbaud to SpeedyBee |
| GP2/GP3 | Encoder CLK/DT | Present in hardware, not used by active firmware |
| GP4 | Encoder push button | Present in hardware, not used for boot mode selection |
| GP5 | Arm toggle switch | Active low, used for boot mode selection and CRSF CH3 |
| GP8/GP9 | INA219 I2C0 SDA/SCL | Present in hardware, not used by active firmware |
| GP16-21 | LCD (DC/CS/CLK/MOSI/RST/BL) | Internal, not on headers |
| GP22 | WS2812 LED | Internal, PIO0 SM0 |
| GP26-29 | ADC0-3 pots | Throttle, Steering, Steering Trim, Throttle Limiter |

**Board headers expose only**: GP0-GP9, GP25-GP29. GP10-GP24 are internal.
**Free pins**: GP6, GP7, GP25

## Key Technical Details

### Display (display.c)
- Must match Waveshare's exact ST7789 init sequence (COLMOD=0x05, specific gamma tables)
- NEVER full-screen clear every frame (takes ~30ms at 30MHz SPI = flicker)
- Use padded text rows (`draw_row()`) instead of fill_rect + draw_string
- Window offset: 34 pixels on Y axis in landscape mode
- Pixels must be byte-swapped (big-endian RGB565)

### WS2812 LED (led.c)
- Must use PIO, NOT bit-bang nops (RP2350 at 150MHz makes nop timing unreliable)
- R and G channels are SWAPPED on this board vs standard WS2812

### INA219 (ina219.c)
- Must use `i2c_write_timeout_us` / `i2c_read_timeout_us` (blocking variants hang forever without device)
- Not part of the active KISS build

### Active Control Behavior
- Steering pot maps to CH1 through hardcoded calibration.
- Throttle pot maps to CH2 through hardcoded calibration.
- Steering trim pot applies a bounded offset to CH1.
- Throttle trim pot is a speed limiter that scales throttle throw from 0% to 100%.
- When disarmed, steering and throttle are centered and only arm state is transmitted.
- Receiver link is only considered up when telemetry is recent and link quality is above zero.

## Project Documentation
- Full project plan: `~/Documents/Mikko notes/personal_projects/RC_FPV/DIY_ELRS_Surface_Radio_-_Project_Plan_v3.md`
- Waveshare LCD demo: `/tmp/waveshare-lcd/RP2350-LCD-1.47/C/01-LCD/`
- Waveshare LVGL demo: `/tmp/waveshare-lvgl/RP2350-LCD-1.47-LVGL/C/`
- Waveshare WS2812 demo: `~/Downloads/RP2350-LCD-1.47/C/03-RGB/`
- Schematic: `~/Downloads/RP2350-LCD-1.47-A(1).pdf`

## Power Architecture
- **Battery Shield V3**: 18650 holder + TP4056 charger + 5V boost + auto-shutoff
- **INA219** (0x40): 5V system side — rail voltage + current + power draw
- **External toggle switch** (with LED) required — Battery Shield V3 switch only controls USB-A port
- Battery Shield auto-shutoff protects cell → no battery-side monitoring needed
- Current KISS build relies on receiver telemetry for battery/link display; INA219 support is inactive.

## Current Status (2026-04-08)
- Active KISS firmware builds cleanly.
- Internal Waveshare LCD is enabled in the active build.
- Boot/drive/WiFi flow is controlled from the arm switch on GP5.
- Steering trim is live on CH1.
- Throttle trim is intentionally a speed limiter.
- Proper internal ELRS settings/config communication is still future work.
