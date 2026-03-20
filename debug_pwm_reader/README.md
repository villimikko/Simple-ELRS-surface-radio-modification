# Debug PWM Reader

Standalone RP2350 firmware that reads PWM pulse widths from an ELRS receiver and outputs them over USB serial. Used to verify the receiver is actually outputting PWM — helped debug the model match issue where the receiver silently ignored CRSF packets and output no PWM.

## Pin Connections

| Pico Pin | Signal |
|----------|--------|
| GP0 | Receiver CH1 (steering) PWM output |
| GP1 | Receiver CH2 (throttle) PWM output |
| GND | Receiver GND |

## Output Format

USB CDC at 10Hz:
```
$P,1500,1500
```
Fields: `ch1_us`, `ch2_us` (pulse width in microseconds).

## Build & Flash

```bash
mkdir build && cd build
cmake ..
make
# Copy debug_pwm_reader.uf2 to the Pico in BOOTSEL mode
```

## What It Helped Debug

The ELRS receiver (HappyModel EP1) was not outputting any PWM signal. This tool confirmed the receiver outputs were flat (0 us). The root cause was **model match** enabled on the receiver — since the handset wasn't sending the correct model ID command frame, the receiver ignored all RC channel data. Fix: disable model match on the receiver via ELRS web UI.
