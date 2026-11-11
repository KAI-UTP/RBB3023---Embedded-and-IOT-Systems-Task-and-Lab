#
# Assignment 1 - laptop viewer for board #2
#
#   Opens the USB serial port of LPC1768 board #2 (115200 baud), reads the lines
#   "RX #123  POT1 = 2048 ( 50%) 1.65V   POT2 = ...  lost 0 bad 0"
#   and shows POT1 / POT2 as live bars in the terminal.
#
#   py asg1_viewer.py          -> finds the mbed port by itself (VID 0x0D28)
#   py asg1_viewer.py COM9     -> use this port
#   Ctrl+C to quit
#
import re
import sys

import serial
import serial.tools.list_ports

BAUD = 115200
BAR = 40
LINE = re.compile(r"RX #(\d+)\s+POT1 = \s*(\d+).*?POT2 = \s*(\d+).*?lost (\d+) bad (\d+)")


def find_port():
	ports = [p.device for p in serial.tools.list_ports.comports() if p.vid == 0x0D28]
	if not ports:
		sys.exit("No mbed serial port found - plug in board #2 or give the COM port")
	return ports[0]


def bar(value):
	n = value * BAR // 4095
	return "#" * n + "." * (BAR - n)


def main():
	port = sys.argv[1] if len(sys.argv) > 1 else find_port()
	print(f"Board #2 on {port} at {BAUD} baud - Ctrl+C to quit\n")

	with serial.Serial(port, BAUD, timeout=1.5) as s:
		while True:
			raw = s.readline().decode(errors="ignore").strip()
			if not raw:
				print("\r  waiting for board #2 ...".ljust(110), end="", flush=True)
				continue
			m = LINE.search(raw)
			if not m:
				print("\r" + raw.ljust(110))		# start-up message etc.
				continue
			n, p1, p2, lost, bad = map(int, m.groups())
			print(f"\rPOT1 {p1:4d} {p1 * 100 // 4095:3d}% [{bar(p1)}]  "
			      f"POT2 {p2:4d} {p2 * 100 // 4095:3d}% [{bar(p2)}]  "
			      f"#{n} lost {lost} bad {bad}   ", end="", flush=True)


if __name__ == "__main__":
	try:
		main()
	except KeyboardInterrupt:
		print()
