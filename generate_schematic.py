#!/usr/bin/env python3
"""Generate ELRS Surface Radio schematic using schemdraw."""
import schemdraw
import schemdraw.elements as elm

OUTPUT = "elrs_surface_radio_schematic.svg"

with schemdraw.Drawing(file=OUTPUT, show=False, fontsize=10, unit=3) as d:

    # ============================================================
    # TITLE
    # ============================================================
    d += elm.Label().at((20, 36)).label("ELRS Surface Radio — Schematic", fontsize=16)
    d += elm.Label().at((20, 34.5)).label(
        "RP2350-LCD-1.47-A  |  v0.2  |  2026-03-17  |  Lazy Build (no ADC RC filters)", fontsize=9)

    # ============================================================
    # POWER CHAIN
    # ============================================================
    d += elm.Label().at((1, 31)).label("POWER CHAIN", fontsize=13)

    # Battery
    d += elm.BatteryCell().at((2, 28)).right().reverse().label("18650\n3.7V", loc="top", fontsize=8)
    d += elm.Line().right(0.5)

    # INA219 #1
    d += elm.RBox(w=3, h=1.5).label("INA219 #1\n0x40 Batt", fontsize=7)
    ina1_end = d.here
    d += elm.Line().right(0.5)

    # Battery Shield V3
    d += elm.RBox(w=3.5, h=1.5).label("Batt Shield V3\nTP4056+5V Boost", fontsize=7)
    d += elm.Line().right(0.5)

    # ON/OFF Switch
    sw_pos = d.here
    d += elm.Switch().right().label("SW1 ON/OFF", loc="top", fontsize=8)
    sw_end = d.here

    # LED on switch (branch up from switch midpoint)
    sw_mid = ((sw_pos[0] + sw_end[0]) / 2, sw_pos[1])
    d += elm.Dot().at(sw_pos)
    d += elm.Line().at(sw_pos).up(1.5)
    d += elm.LED().right(2).label("5V LED", loc="top", fontsize=7)
    d += elm.Line().down(1.5)

    # Continue from switch
    d += elm.Line().at(sw_end).right(0.5)

    # INA219 #2
    d += elm.RBox(w=3, h=1.5).label("INA219 #2\n0x41 5V", fontsize=7)
    d += elm.Line().right(0.5)
    vsys_pt = d.here

    # VSYS label
    d += elm.Label().label("VSYS", loc="right", fontsize=10)

    # Branch down to SpeedyBee 5V
    d += elm.Dot().at(vsys_pt)
    d += elm.Line().at(vsys_pt).down(1.5)
    d += elm.Label().label("SpeedyBee 5V", loc="bottom", fontsize=8)

    # GND under battery
    d += elm.Line().at((2, 28)).down(1.5)
    d += elm.Ground()

    # I2C bus note
    d += elm.Label().at((6, 30)).label("Both INA219: I2C0 bus GP8(SDA) / GP9(SCL)", fontsize=7)

    # ============================================================
    # POTENTIOMETERS
    # ============================================================
    pot_x = 33
    d += elm.Label().at((pot_x, 31)).label("ADC INPUTS (direct wire)", fontsize=13)

    # 3.3V rail
    rail_x = pot_x
    d += elm.Line().at((rail_x, 29)).down(14)
    d += elm.Label().at((rail_x - 0.3, 29)).label("+3V3", loc="left", fontsize=9)

    # GND rail
    gnd_x = pot_x + 7
    d += elm.Line().at((gnd_x, 29)).down(14)
    d += elm.Ground().at((gnd_x, 15))

    pots = [
        ("RV1", "Throttle\n(spring)", "GP26"),
        ("RV2", "Steering\n(spring)", "GP27"),
        ("RV3", "Steer Trim", "GP28"),
        ("RV4", "Throt Trim", "GP29"),
    ]

    for i, (ref, desc, gpio) in enumerate(pots):
        py = 28 - i * 3.2

        # Connect from 3.3V rail
        d += elm.Dot().at((rail_x, py))

        # Potentiometer
        pot = d.add(elm.Potentiometer().at((rail_x + 0.5, py)).right(4).label(
            f"{ref} {desc}", loc="bottom", fontsize=7))

        # Connect to GND rail
        d += elm.Dot().at((gnd_x, py))

        # Wiper output → GPIO
        d += elm.Line().at(pot.tap).up(1.2)
        d += elm.Line().right(1.5)
        d += elm.Label().label(f"→ {gpio}", loc="right", fontsize=9)

    # ============================================================
    # ROTARY ENCODER
    # ============================================================
    enc_x = 1
    enc_y = 20
    d += elm.Label().at((enc_x, enc_y + 2)).label("ROTARY ENCODER (KY-040)", fontsize=11)

    # Encoder box
    d += elm.RBox(w=4, h=3.5).at((enc_x + 0.5, enc_y - 0.5)).label("KY-040\nEncoder", fontsize=9)

    # CLK with cap
    d += elm.Line().at((enc_x + 0.5, enc_y + 0.5)).left(2)
    clk_pt = d.here
    d += elm.Dot()
    d += elm.Label().label("GP2 ←", loc="left", fontsize=9)
    d += elm.Capacitor().at(clk_pt).down(1.5).label("C1 50nF", loc="right", fontsize=7)
    d += elm.Ground()

    # DT with cap
    d += elm.Line().at((enc_x + 0.5, enc_y - 0.5)).left(2)
    dt_pt = d.here
    d += elm.Dot()
    d += elm.Label().label("GP3 ←", loc="left", fontsize=9)
    d += elm.Capacitor().at(dt_pt).down(1.5).label("C2 50nF", loc="right", fontsize=7)
    d += elm.Ground()

    # SW
    d += elm.Line().at((enc_x + 0.5, enc_y - 1.5)).left(2)
    d += elm.Label().label("GP4 ←", loc="left", fontsize=9)

    # Encoder power
    d += elm.Line().at((enc_x + 4.5, enc_y + 0.5)).right(1)
    d += elm.Label().label("+3V3", loc="right", fontsize=9)
    d += elm.Line().at((enc_x + 4.5, enc_y - 1.5)).right(1)
    d += elm.Ground()

    # ============================================================
    # TOGGLE SWITCH (CH3)
    # ============================================================
    sw2_x = 1
    sw2_y = 13
    d += elm.Label().at((sw2_x, sw2_y + 1)).label("CH3 SWITCH", fontsize=11)
    d += elm.Label().at((sw2_x - 0.5, sw2_y)).label("GP5 →", loc="left", fontsize=9)
    d += elm.Switch().at((sw2_x, sw2_y)).right(3).label("SW2 Toggle", loc="top", fontsize=8)
    d += elm.Ground()

    # ============================================================
    # 3V3 DECOUPLING
    # ============================================================
    dec_x = 1
    dec_y = 10
    d += elm.Label().at((dec_x, dec_y + 1)).label("3V3 DECOUPLING", fontsize=11)
    d += elm.Label().at((dec_x - 0.5, dec_y)).label("+3V3 →", loc="left", fontsize=9)
    d += elm.Capacitor().at((dec_x, dec_y)).right(3).label("C3 100nF", loc="top", fontsize=8)
    d += elm.Ground()

    # ============================================================
    # SPEEDYBEE NANO
    # ============================================================
    spb_x = 16
    spb_y = 20
    d += elm.Label().at((spb_x, spb_y + 3)).label("RF MODULE", fontsize=13)

    d += elm.RBox(w=5, h=3).at((spb_x, spb_y)).label(
        "SpeedyBee Nano\n2.4GHz ELRS TX\n420kbaud CRSF", fontsize=8)

    # UART pins
    d += elm.Line().at((spb_x, spb_y + 0.7)).left(2)
    d += elm.Label().label("GP0 TX →", loc="left", fontsize=8)

    d += elm.Line().at((spb_x, spb_y - 0.7)).left(2)
    d += elm.Label().label("GP1 RX ←", loc="left", fontsize=8)

    # Power pins
    d += elm.Line().at((spb_x + 5, spb_y + 0.7)).right(1)
    d += elm.Label().label("5V", loc="right", fontsize=9)

    d += elm.Line().at((spb_x + 5, spb_y - 0.7)).right(1)
    d += elm.Ground()

    # Antenna
    d += elm.Line().at((spb_x + 2.5, spb_y + 1.5)).up(1.5)
    d += elm.Antenna().label("2.4GHz", fontsize=8)

    # ============================================================
    # ER5A V2 (vehicle side)
    # ============================================================
    rx_x = 16
    rx_y = 13
    d += elm.Label().at((rx_x, rx_y + 2.5)).label("VEHICLE SIDE", fontsize=13)

    d += elm.RBox(w=5, h=2.5).at((rx_x, rx_y)).label(
        "RadioMaster ER5A V2\n5ch PWM ELRS RX", fontsize=8)

    # Outputs
    d += elm.Line().at((rx_x + 5, rx_y + 0.5)).right(1.5)
    d += elm.Label().label("CH1→Servo", loc="right", fontsize=8)

    d += elm.Line().at((rx_x + 5, rx_y - 0.5)).right(1.5)
    d += elm.Label().label("CH2→ESC", loc="right", fontsize=8)

    # Wireless link indicator
    d += elm.Label().at((spb_x + 2.5, spb_y - 2.5)).label("~ ~ ~ 2.4GHz ~ ~ ~", fontsize=8)

    # ============================================================
    # INTERNAL PERIPHERALS NOTE
    # ============================================================
    int_x = 16
    int_y = 9
    d += elm.Label().at((int_x, int_y + 1.5)).label("INTERNAL (no wiring needed)", fontsize=11)
    d += elm.RBox(w=6, h=2).at((int_x + 0.5, int_y - 0.5)).label(
        "ST7789 LCD 172x320 (GP16-21 SPI0)\nWS2812 RGB LED (GP22 PIO)", fontsize=7)

    # ============================================================
    # NOTES
    # ============================================================
    d += elm.Label().at((1, 6)).label(
        "Notes:", fontsize=9)
    d += elm.Label().at((1, 5)).label(
        "- CRSF UART0 @ 420kbaud (8N1, non-inverted) on GP0/GP1", fontsize=7)
    d += elm.Label().at((1, 4.2)).label(
        "- I2C0 @ 100kHz on GP8/GP9 (shared by both INA219, 50ms timeout)", fontsize=7)
    d += elm.Label().at((1, 3.4)).label(
        "- ADC: 12-bit, 16x oversample, EMA smooth, 8-count deadband at center", fontsize=7)
    d += elm.Label().at((1, 2.6)).label(
        "- Lazy build: no RC filters on ADC inputs. Upgrade: add 1k + 100nF per pot", fontsize=7)
    d += elm.Label().at((1, 1.8)).label(
        "- Battery Shield V3 switch only controls USB-A port. External SW1 for power off", fontsize=7)

print(f"Schematic saved to {OUTPUT}")
