# CLAUDE.md — ELRS Surface Radio Firmware

## Project Overview

DIY 2.4GHz ELRS surface radio controller for RC cars. Runs on **Waveshare RP2350-LCD-1.47-A** board with onboard 1.47" ST7789 LCD and WS2812 RGB LED. Communicates with SpeedyBee Nano RX (flashed as TX module) over CRSF protocol to RadioMaster ER5A V2 receiver.

## Build

```bash
cd build
cmake ..
make -j$(nproc)
# Flash: hold BOOT + press RESET on board, then:
cp build/elrs-surface-radio.uf2 /media/mikko-ultrawork/RP2350/
```

SDK: Pico SDK 2.2.0, Board: pico2 (RP2350)

## Architecture

- **Core 0**: Real-time CRSF loop at 250Hz — ADC read, mixer chain, CRSF TX/RX, LED update
- **Core 1**: Display + UI at 15Hz — encoder input, menu state machine, page rendering, INA219
- **Inter-core**: Shared state protected by spin lock (`shared.c`)

## Source Files (src/)

| File | Purpose |
|------|---------|
| main.c | Dual-core entry, init sequence, main loops |
| crsf.c/h | CRSF protocol: 420kbaud UART, RC channels TX, telemetry RX |
| adc.c/h | 4-channel ADC with 16x oversampling + EMA smoothing |
| mixer.c/h | Full mixer chain: calibrate → trim → subtrim → deadband → expo → DR → EPA |
| display.c/h | ST7789 LCD driver: SPI0 at 30MHz, landscape 320x172, 8x16 bitmap font |
| pages.c/h | 3 driving pages + menu rendering + calibration screen |
| menu.c/h | Menu state machine: driving → main → steering/throttle → model select → calibrate |
| encoder.c/h | GPIO polling rotary encoder with gray code lookup + button debounce |
| buttons.c/h | Toggle switch on GP5 (debounced, maps to CRSF CH3) |
| calibrate.c/h | 3-point calibration wizard (min/center/max, 64-sample averaging) |
| config.c/h | Flash storage: 4 model profiles with all mixer settings |
| ina219.c/h | INA219 power monitor: 0x40 (5V system side), I2C timeout |
| led.c/h | WS2812 via PIO (ws2812.pio) — red=no link, green=linked, orange=starting |
| shared.c/h | Inter-core shared state with spin lock |
| hardware_config.h | Central pin assignment and CRSF channel mapping config |
| ws2812.pio | PIO program for WS2812 LED (4 instructions) |

## Pin Assignments (hardware_config.h)

| GPIO | Function | Notes |
|------|----------|-------|
| GP0/GP1 | CRSF UART TX/RX | 420kbaud to SpeedyBee |
| GP2/GP3 | Encoder CLK/DT | GPIO polling + 50nF debounce caps |
| GP4 | Encoder push button | Active low, short/long press |
| GP5 | Toggle switch | Active low → CRSF CH3 |
| GP8/GP9 | INA219 I2C0 SDA/SCL | 0x40, 5V system side |
| GP16-21 | LCD (DC/CS/CLK/MOSI/RST/BL) | Internal, not on headers |
| GP22 | WS2812 LED | Internal, PIO0 SM0 |
| GP26-29 | ADC0-3 pots | Throttle, Steering, Trim Steer, Trim Throt |

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

### Mixer Chain Order (mixer.c)
```
Raw ADC → 16x oversample → EMA smooth → Calibrate (min/center/max)
→ Add trim pot → Add subtrim → Deadband (8 counts, spring pots only)
→ Expo curve → Dual rate → EPA (asymmetric) → Map to CRSF (172-1811)
```

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

## Current Status (2026-03-18)
All firmware modules written and compiling. LCD display and WS2812 LED verified working on hardware. Single INA219 on 5V system side. Next step is wiring pots, encoder, SpeedyBee, INA219, and testing end-to-end.
