#!/usr/bin/env python3
"""ELRS Surface Radio — Drive Monitor GUI
Format: $D,sent0-4,mixer0-4,rssi,lq,link,vbat,uptime,tx
"""

import glob
import serial
import threading
import tkinter as tk

# Auto-detect port
ports = sorted(glob.glob("/dev/ttyACM*"))
PORT = ports[0] if ports else "/dev/ttyACM0"
BAUD = 115200

CRSF_MIN = 172
CRSF_CENTER = 992
CRSF_MAX = 1811


def crsf_to_pwm(crsf_val):
    """Convert CRSF value (172-1811) to PWM microseconds (1000-2000)"""
    return 1000 + (crsf_val - CRSF_MIN) * 1000 // (CRSF_MAX - CRSF_MIN)


def crsf_to_pct(crsf_val):
    """Convert CRSF to percentage from center (-100 to +100)"""
    return (crsf_val - CRSF_CENTER) * 100 // (CRSF_MAX - CRSF_CENTER)


CH_NAMES = ["CH1 Steering", "CH2 Throttle", "CH3 ArmSwitch", "CH4 TrimSteer", "CH5 TrimThrot"]
CH_COLORS = ["#2196F3", "#4CAF50", "#FF5722", "#FF9800", "#9C27B0"]


class DriveGUI:
    def __init__(self, root):
        self.root = root
        root.title("ELRS Drive Monitor")
        root.configure(bg="#1e1e1e")
        root.geometry("800x680")

        # Data
        self.sent = [CRSF_CENTER] * 5
        self.mixer = [CRSF_CENTER] * 5
        self.armed = False
        self.rssi = 0
        self.lq = 0
        self.link = False
        self.vbat = 0
        self.uptime = 0
        self.crsf_tx = 0

        # Header
        tk.Label(root, text="ELRS Surface Radio",
                 font=("monospace", 16, "bold"), fg="white", bg="#1e1e1e").pack(pady=(10, 0))

        # Link status bar
        self.link_frame = tk.Frame(root, bg="#1e1e1e")
        self.link_frame.pack(fill="x", padx=20, pady=5)

        self.link_lbl = tk.Label(self.link_frame, text="NO LINK",
                                  font=("monospace", 13, "bold"), fg="#f44336", bg="#1e1e1e")
        self.link_lbl.pack(side="left")

        self.rssi_lbl = tk.Label(self.link_frame, text="RSSI: --",
                                  font=("monospace", 11), fg="#888", bg="#1e1e1e")
        self.rssi_lbl.pack(side="left", padx=20)

        self.lq_lbl = tk.Label(self.link_frame, text="LQ: --",
                                font=("monospace", 11), fg="#888", bg="#1e1e1e")
        self.lq_lbl.pack(side="left", padx=10)

        self.vbat_lbl = tk.Label(self.link_frame, text="VBat: --",
                                  font=("monospace", 11), fg="#888", bg="#1e1e1e")
        self.vbat_lbl.pack(side="left", padx=10)

        self.arm_lbl = tk.Label(self.link_frame, text="DISARMED",
                                 font=("monospace", 12, "bold"), fg="#f44336", bg="#1e1e1e")
        self.arm_lbl.pack(side="right")

        # Section: CRSF Output (what receiver gets)
        tk.Label(root, text="CRSF Output (to receiver)",
                 font=("monospace", 11), fg="#aaa", bg="#1e1e1e", anchor="w").pack(fill="x", padx=20, pady=(10, 2))

        self.sent_bars = []
        self.sent_labels = []
        sent_frame = tk.Frame(root, bg="#1e1e1e")
        sent_frame.pack(fill="x", padx=20)

        for i in range(5):
            frame = tk.Frame(sent_frame, bg="#2d2d2d")
            frame.pack(fill="x", pady=2, ipady=3, ipadx=8)

            top = tk.Frame(frame, bg="#2d2d2d")
            top.pack(fill="x")

            tk.Label(top, text=CH_NAMES[i], font=("monospace", 10, "bold"),
                     fg=CH_COLORS[i], bg="#2d2d2d", width=16, anchor="w").pack(side="left")

            lbl = tk.Label(top, text="992 (1500us) 0%",
                           font=("monospace", 10), fg="white", bg="#2d2d2d")
            lbl.pack(side="right")
            self.sent_labels.append(lbl)

            canvas = tk.Canvas(frame, height=12, bg="#444", highlightthickness=0)
            canvas.pack(fill="x", padx=4, pady=(2, 1))
            self.sent_bars.append(canvas)

        # Section: Mixer Output (before arm gate)
        tk.Label(root, text="Mixer Output (before arm gate)",
                 font=("monospace", 11), fg="#aaa", bg="#1e1e1e", anchor="w").pack(fill="x", padx=20, pady=(10, 2))

        self.mix_bars = []
        self.mix_labels = []
        mix_frame = tk.Frame(root, bg="#1e1e1e")
        mix_frame.pack(fill="x", padx=20)

        for i in range(5):
            frame = tk.Frame(mix_frame, bg="#2d2d2d")
            frame.pack(fill="x", pady=2, ipady=3, ipadx=8)

            top = tk.Frame(frame, bg="#2d2d2d")
            top.pack(fill="x")

            tk.Label(top, text=CH_NAMES[i], font=("monospace", 10),
                     fg="#777", bg="#2d2d2d", width=16, anchor="w").pack(side="left")

            lbl = tk.Label(top, text="992 (1500us)",
                           font=("monospace", 10), fg="#aaa", bg="#2d2d2d")
            lbl.pack(side="right")
            self.mix_labels.append(lbl)

            canvas = tk.Canvas(frame, height=10, bg="#444", highlightthickness=0)
            canvas.pack(fill="x", padx=4, pady=(2, 1))
            self.mix_bars.append(canvas)

        # Stats
        self.stats_lbl = tk.Label(root, text="Uptime: -- | TX: --",
                                   font=("monospace", 10), fg="#666", bg="#1e1e1e")
        self.stats_lbl.pack(pady=8)

        # Status
        self.status = tk.Label(root, text="Connecting...", font=("monospace", 9),
                               fg="#555", bg="#1e1e1e")
        self.status.pack(side="bottom", pady=3)

        self.running = True
        self.thread = threading.Thread(target=self.serial_reader, daemon=True)
        self.thread.start()
        self.update_ui()

    def serial_reader(self):
        import time
        while self.running:
            try:
                # Re-detect port each attempt
                ports = sorted(glob.glob("/dev/ttyACM*"))
                port = ports[0] if ports else PORT
                ser = serial.Serial(port, BAUD, timeout=1)
                self.root.after(0, lambda p=port: self.status.config(text=f"Connected: {p}"))
                while self.running:
                    line = ser.readline().decode("utf-8", errors="ignore").strip()
                    if not line.startswith("$D,"):
                        continue
                    parts = line.split(",")
                    if len(parts) < 16:
                        continue
                    # $D,sent0-4,mixer0-4,rssi,lq,link,vbat,uptime,tx
                    try:
                        self.sent = [int(parts[i]) for i in range(1, 6)]
                        self.mixer[0] = int(parts[6])
                        self.mixer[1] = int(parts[7])
                        self.armed = parts[8] == "1"
                        self.mixer[2] = CRSF_MAX if self.armed else CRSF_MIN
                        self.mixer[3] = int(parts[9])
                        self.mixer[4] = int(parts[10])
                        self.rssi = int(parts[11])
                        self.lq = int(parts[12])
                        self.link = parts[13] == "1"
                        self.vbat = int(parts[14])
                        self.uptime = int(parts[15])
                        if len(parts) > 16:
                            self.crsf_tx = int(parts[16])
                    except (ValueError, IndexError):
                        continue
            except (serial.SerialException, OSError):
                self.root.after(0, lambda: self.status.config(text="Disconnected — retrying..."))
                time.sleep(2)
            except (ValueError, IndexError):
                # Bad line, skip
                pass

    def draw_bar(self, canvas, val, color):
        canvas.delete("all")
        w = canvas.winfo_width()
        h = canvas.winfo_height()
        if w < 10:
            w = 700

        # Center line
        x_center = w // 2
        canvas.create_line(x_center, 0, x_center, h, fill="#666", width=1)

        # Position
        frac = (val - CRSF_MIN) / max(1, CRSF_MAX - CRSF_MIN)
        frac = max(0.0, min(1.0, frac))
        x_pos = int(frac * w)

        # Fill from center
        if x_pos < x_center:
            canvas.create_rectangle(x_pos, 1, x_center, h - 1, fill=color, outline="")
        else:
            canvas.create_rectangle(x_center, 1, x_pos, h - 1, fill=color, outline="")

        # Marker
        bw = max(2, int(w * 0.005))
        canvas.create_rectangle(x_pos - bw, 0, x_pos + bw, h, fill="white", outline="")

    def update_ui(self):
        # Link status
        if self.link:
            self.link_lbl.config(text="LINKED", fg="#4CAF50")
            self.rssi_lbl.config(text=f"RSSI: {self.rssi}dBm", fg="#aaa")
            self.lq_lbl.config(text=f"LQ: {self.lq}%", fg="#aaa")
        else:
            self.link_lbl.config(text="NO LINK", fg="#f44336")
            self.rssi_lbl.config(text="RSSI: --", fg="#555")
            self.lq_lbl.config(text="LQ: --", fg="#555")

        if self.vbat > 0:
            v = self.vbat / 1000
            self.vbat_lbl.config(text=f"VBat: {v:.1f}V", fg="#aaa")
        else:
            self.vbat_lbl.config(text="VBat: --", fg="#555")

        if self.armed:
            self.arm_lbl.config(text="ARMED", fg="#4CAF50")
        else:
            self.arm_lbl.config(text="DISARMED", fg="#f44336")

        # CRSF sent bars
        for i in range(5):
            val = self.sent[i]
            pwm = crsf_to_pwm(val)
            pct = crsf_to_pct(val)
            if i == 2:  # arm switch
                txt = f"{val} ({pwm}us) {'ARM' if val > CRSF_CENTER else 'SAFE'}"
            else:
                txt = f"{val} ({pwm}us) {pct:+d}%"
            self.sent_labels[i].config(text=txt)
            self.draw_bar(self.sent_bars[i], val, CH_COLORS[i])

        # Mixer bars
        for i in range(5):
            val = self.mixer[i]
            pwm = crsf_to_pwm(val)
            if i == 2:
                txt = f"{'ARMED' if self.armed else 'DISARMED'}"
            else:
                txt = f"{val} ({pwm}us)"
            self.mix_labels[i].config(text=txt)
            self.draw_bar(self.mix_bars[i], val, "#666")

        # Stats
        m, s = divmod(self.uptime, 60)
        h, m = divmod(m, 60)
        self.stats_lbl.config(text=f"Uptime: {h:02d}:{m:02d}:{s:02d}  |  TX: {self.crsf_tx:,}")

        self.root.after(50, self.update_ui)

    def stop(self):
        self.running = False


def main():
    root = tk.Tk()
    app = DriveGUI(root)
    root.protocol("WM_DELETE_WINDOW", lambda: (app.stop(), root.destroy()))
    root.mainloop()


if __name__ == "__main__":
    main()
