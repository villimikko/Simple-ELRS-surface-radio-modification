Current firmware status:

- Active firmware is the KISS build with the internal Waveshare ST7789 RGB LCD enabled.
- Boot flow is `ARM ON -> DRIVE` and `ARM OFF -> WiFi standby after 60 seconds`.
- Steering trim offsets CH1 directly.
- Throttle trim is intentionally a speed limiter, not a throttle-center trim.
- Basic drive, screen, telemetry display, and WiFi standby flow are working in the active build.

Still future work:

- Proper internal ELRS settings/config communication similar to the EdgeTX Lua flow is not implemented yet.
