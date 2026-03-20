#ifndef HARDWARE_CONFIG_H
#define HARDWARE_CONFIG_H

// ============================================================================
// ELRS Surface Radio — Hardware & Channel Configuration
//
// Edit this file to change pin assignments, channel mapping, and input roles.
// Everything in one place. Rebuild after changes.
// ============================================================================

// --- CRSF UART (to SpeedyBee TX module) ---
#define PIN_CRSF_TX         0   // GP0 → SpeedyBee RX pad
#define PIN_CRSF_RX         1   // GP1 → SpeedyBee TX pad

// --- Analog inputs (pots, active on GP26-GP29) ---
//     ADC channel 0 = GP26, 1 = GP27, 2 = GP28, 3 = GP29
#define PIN_POT_THROTTLE    26  // GP26 — main throttle stick (spring-return)
#define PIN_POT_STEERING    27  // GP27 — main steering wheel (spring-return)
#define PIN_POT_TRIM_STEER  28  // GP28 — steering trim knob (no spring)
#define PIN_POT_TRIM_THROT  29  // GP29 — throttle trim knob (no spring)

// --- Rotary encoder (KY-040, with 50nF debounce caps on CLK/DT) ---
#define PIN_ENC_CLK         2   // GP2 — encoder A (quadrature)
#define PIN_ENC_DT          3   // GP3 — encoder B (quadrature)
#define PIN_ENC_SW          4   // GP4 — encoder push button (active low)

// --- Toggle switch ---
#define PIN_SWITCH1         5   // GP5 — on/off toggle (active low)

// --- INA219 power monitor (I2C0) ---
#define PIN_INA219_SDA      8   // GP8 (I2C0 SDA)
#define PIN_INA219_SCL      9   // GP9 (I2C0 SCL)
#define INA219_ADDR         0x40  // Default: A0=GND, A1=GND

// --- LCD (ST7789 on SPI0, Waveshare RP2350-LCD-1.47-A pinout) ---
#define PIN_LCD_DC          16  // GP16 — data/command select
#define PIN_LCD_CS          17  // GP17 — SPI0 CSn
#define PIN_LCD_CLK         18  // GP18 — SPI0 SCK
#define PIN_LCD_MOSI        19  // GP19 — SPI0 TX (DIN)
#define PIN_LCD_RST         20  // GP20 — reset
#define PIN_LCD_BL          21  // GP21 — backlight (PWM)

// --- Onboard WS2812 RGB LED ---
#define PIN_LED_WS2812      22  // GP22 — Waveshare onboard LED


// ============================================================================
// CRSF CHANNEL MAPPING
//
// Which physical input goes to which CRSF channel (1-16).
// CRSF channels are 1-indexed in radio terminology, 0-indexed in the array.
// ============================================================================

// Analog channels (mixed through calibration + expo + DR + EPA)
#define CH_STEERING         1   // CRSF CH1 — steering
#define CH_THROTTLE         2   // CRSF CH2 — throttle

// Switch channels (raw on/off, mapped to CRSF min/max)
#define CH_SWITCH1          3   // CRSF CH3 — toggle switch on GP5

// Unused channels default to CRSF center (992).
// To add more switches or aux channels, define them here and
// wire them in main.c where channels[] is built.
//
// Examples for future expansion:
// #define CH_AUX1          4   // CRSF CH4 — second toggle switch
// #define CH_AUX2          5   // CRSF CH5 — potentiometer


// ============================================================================
// SWITCH BEHAVIOR
//
// How each switch maps to CRSF values.
// ============================================================================

// 2-position toggle: ON = max (1811), OFF = min (172)
#define SWITCH_ON_VALUE     1811
#define SWITCH_OFF_VALUE    172

// If you want a 3-position switch in the future:
// #define SWITCH_MID_VALUE 992


#endif
