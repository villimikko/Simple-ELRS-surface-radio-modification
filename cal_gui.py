#!/usr/bin/env python3
"""ELRS Surface Radio — Calibration GUI
Reads $C (live ADC) and $R (min/max range) lines from serial.
Shows bars, numbers, and switch state.
"""

import sys
import serial
import threading
import tkinter as tk

PORT = "/dev/ttyACM0"
BAUD = 115200
ADC_MAX = 4095

LABELS = ["Steering", "Throttle", "Trim Steer", "Trim Throttle"]
COLORS = ["#2196F3", "#4CAF50", "#FF9800", "#9C27B0"]


class CalGUI:
    def __init__(self, root):
        self.root = root
        root.title("ELRS Calibration")
        root.configure(bg="#1e1e1e")
        root.geometry("700x500")

        # Data
        self.live = [0, 0, 0, 0]
        self.mins = [4095, 4095, 4095, 4095]
        self.maxs = [0, 0, 0, 0]
        self.center = [0, 0, 0, 0]
        self.switch = False
        self.center_set = False

        # Header
        tk.Label(root, text="ELRS Surface Radio — Calibration",
                 font=("monospace", 16, "bold"), fg="white", bg="#1e1e1e").pack(pady=10)

        # Pot frames
        self.bars = []
        self.val_labels = []
        self.range_labels = []
        self.center_labels = []

        pots_frame = tk.Frame(root, bg="#1e1e1e")
        pots_frame.pack(fill="x", padx=20)

        for i in range(4):
            frame = tk.Frame(pots_frame, bg="#2d2d2d", relief="flat", bd=0)
            frame.pack(fill="x", pady=4, ipady=6, ipadx=10)

            # Name + value
            top = tk.Frame(frame, bg="#2d2d2d")
            top.pack(fill="x")

            tk.Label(top, text=LABELS[i], font=("monospace", 12, "bold"),
                     fg=COLORS[i], bg="#2d2d2d", width=15, anchor="w").pack(side="left")

            val_lbl = tk.Label(top, text="0", font=("monospace", 14),
                               fg="white", bg="#2d2d2d", width=6)
            val_lbl.pack(side="left")
            self.val_labels.append(val_lbl)

            range_lbl = tk.Label(top, text="min:---- max:----",
                                 font=("monospace", 10), fg="#888", bg="#2d2d2d")
            range_lbl.pack(side="right")
            self.range_labels.append(range_lbl)

            center_lbl = tk.Label(top, text="ctr:----",
                                  font=("monospace", 10), fg="#666", bg="#2d2d2d")
            center_lbl.pack(side="right", padx=(0, 15))
            self.center_labels.append(center_lbl)

            # Bar canvas
            canvas = tk.Canvas(frame, height=20, bg="#444", highlightthickness=0)
            canvas.pack(fill="x", padx=5, pady=(4, 2))
            self.bars.append(canvas)

        # Switch
        sw_frame = tk.Frame(root, bg="#1e1e1e")
        sw_frame.pack(pady=10)
        tk.Label(sw_frame, text="Switch:", font=("monospace", 12),
                 fg="white", bg="#1e1e1e").pack(side="left")
        self.sw_label = tk.Label(sw_frame, text="OFF", font=("monospace", 14, "bold"),
                                 fg="#f44336", bg="#1e1e1e")
        self.sw_label.pack(side="left", padx=10)

        # Status
        self.status = tk.Label(root, text="Connecting...", font=("monospace", 10),
                               fg="#888", bg="#1e1e1e")
        self.status.pack(side="bottom", pady=5)

        # Summary button
        tk.Button(root, text="Print Summary", font=("monospace", 11),
                  command=self.print_summary, bg="#333", fg="white",
                  activebackground="#555", activeforeground="white",
                  relief="flat", padx=15, pady=5).pack(pady=5)

        # Serial thread
        self.running = True
        self.thread = threading.Thread(target=self.serial_reader, daemon=True)
        self.thread.start()

        # UI update
        self.update_ui()

    def serial_reader(self):
        while self.running:
            try:
                ser = serial.Serial(PORT, BAUD, timeout=1)
                self.root.after(0, lambda: self.status.config(text=f"Connected: {PORT}"))
                while self.running:
                    line = ser.readline().decode("utf-8", errors="ignore").strip()
                    if line.startswith("$C,"):
                        parts = line.split(",")
                        if len(parts) >= 6:
                            self.live = [int(parts[i]) for i in range(1, 5)]
                            self.switch = parts[5] == "1"
                    elif line.startswith("$R,"):
                        parts = line.split(",")
                        if len(parts) >= 9:
                            for i in range(4):
                                self.mins[i] = int(parts[1 + i * 2])
                                self.maxs[i] = int(parts[2 + i * 2])
                    elif line.startswith("$CENTER,"):
                        parts = line.split(",")
                        if len(parts) >= 5:
                            self.center = [int(parts[i]) for i in range(1, 5)]
                            self.center_set = True
            except serial.SerialException:
                self.root.after(0, lambda: self.status.config(text="Disconnected — retrying..."))
                import time
                time.sleep(2)

    def update_ui(self):
        for i in range(4):
            # Value label
            self.val_labels[i].config(text=str(self.live[i]))

            # Range label
            self.range_labels[i].config(
                text=f"min:{self.mins[i]:4d}  max:{self.maxs[i]:4d}")

            # Center label
            if self.center_set:
                self.center_labels[i].config(text=f"ctr:{self.center[i]:4d}")

            # Bar
            canvas = self.bars[i]
            canvas.delete("all")
            w = canvas.winfo_width()
            h = canvas.winfo_height()
            if w < 10:
                w = 600

            # Background with range
            if self.maxs[i] > self.mins[i]:
                x_min = int(self.mins[i] / ADC_MAX * w)
                x_max = int(self.maxs[i] / ADC_MAX * w)
                canvas.create_rectangle(x_min, 0, x_max, h, fill="#555", outline="")

            # Center marker
            if self.center_set and self.center[i] > 0:
                x_ctr = int(self.center[i] / ADC_MAX * w)
                canvas.create_line(x_ctr, 0, x_ctr, h, fill="#FFD700", width=2)

            # Current position
            x_pos = int(self.live[i] / ADC_MAX * w)
            bar_w = max(4, int(w * 0.01))
            canvas.create_rectangle(x_pos - bar_w, 0, x_pos + bar_w, h,
                                    fill=COLORS[i], outline="")

        # Switch
        if self.switch:
            self.sw_label.config(text="ON", fg="#4CAF50")
        else:
            self.sw_label.config(text="OFF", fg="#f44336")

        self.root.after(50, self.update_ui)

    def print_summary(self):
        print("\n" + "=" * 50)
        print("CALIBRATION SUMMARY")
        print("=" * 50)
        for i in range(4):
            print(f"  {LABELS[i]:15s}  center={self.center[i]:4d}  "
                  f"min={self.mins[i]:4d}  max={self.maxs[i]:4d}")
        print(f"  {'Switch':15s}  {'ON' if self.switch else 'OFF'}")
        print("=" * 50)
        print("\nHardcode values for config.h:")
        print("static const cal_pot_t hardcoded_cal[4] = {")
        for i in range(4):
            print(f"    {{ .min = {self.mins[i]:4d}, "
                  f".center = {self.center[i]:4d}, "
                  f".max = {self.maxs[i]:4d} }},  // {LABELS[i]}")
        print("};")
        print()

    def stop(self):
        self.running = False


def main():
    root = tk.Tk()
    app = CalGUI(root)
    root.protocol("WM_DELETE_WINDOW", lambda: (app.stop(), root.destroy()))
    root.mainloop()


if __name__ == "__main__":
    main()
