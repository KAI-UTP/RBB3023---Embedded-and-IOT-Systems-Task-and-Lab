#
# Task 2A (extra) : move the cross hair on the Nu-LB-NUC140 LCD with the laptop mouse
#         laptop mouse -> OpenCV window -> CH340 USB-TTL -> UART1 -> board
#
#   the window is the LCD (128 x 64) drawn 6 times bigger
#     move the mouse inside the window -> cross hair goes there
#     left click                       -> board shows X, Y on the LCD for 3 s
#     Q / ESC                          -> quit
#   the cross hair in the window is where the BOARD says it is,
#   so keypad moves on the board are shown here too
#
#   to board   : 0xAA, x (0~127), y (0~63), flag (1 = click, 0 = move)
#   from board : "P x y\n" cross hair moved, "C x y\n" click position
#
#   usage :
#     py task2a_mouse.py                  find the CH340 port by itself
#     py task2a_mouse.py --port COM5      choose the port
#     py task2a_mouse.py --no-board       try the window without the board
#
import argparse
import threading
import time

import cv2
import numpy as np
import serial
import serial.tools.list_ports

W, H = 128, 64
SCALE = 6
RADIUS, ARM = 9, 13			# same cross hair as the board
BAUD = 115200
CH340_VID = 0x1A86
SEND_GAP = 0.02				# send at most one move every 20 ms (one board frame)
SHOW_TIME = 3.0				# click text shown for 3 s, same as the board
WIN = "Task 2A : mouse -> LCD"


# ---------------- board link ----------------
class Board:
	def __init__(self, port, baud):
		self.ser = serial.Serial(port, baud, timeout=0.05)
		self.pos = (W // 2, H // 2)		# cross hair position from the board
		self.clicks = []
		self.lock = threading.Lock()
		threading.Thread(target=self.reader, daemon=True).start()

	def reader(self):					# runs in the background
		line = b""
		while True:
			try:
				data = self.ser.read(64)
			except serial.SerialException:
				return
			for c in data:
				if c != ord("\n"):
					line += bytes([c])
					continue
				parts = line.decode(errors="ignore").split()
				line = b""
				if len(parts) == 3 and parts[0] in ("P", "C") \
						and parts[1].isdigit() and parts[2].isdigit():
					x, y = int(parts[1]), int(parts[2])
					with self.lock:
						self.pos = (x, y)
						if parts[0] == "C":
							self.clicks.append((x, y))

	def send(self, x, y, click):
		self.ser.write(bytes([0xAA, x, y, 1 if click else 0]))

	def take(self):					# board position, clicks since last call
		with self.lock:
			clicks, self.clicks = self.clicks, []
			return self.pos, clicks


class NoBoard:						# --no-board : keep the position here
	def __init__(self):
		self.pos = (W // 2, H // 2)
		self.clicks = []

	def send(self, x, y, click):
		x = min(max(x, ARM), W - 1 - ARM)	# same limits as the board
		y = min(max(y, ARM), H - 1 - ARM)
		self.pos = (x, y)
		if click:
			self.clicks.append((x, y))

	def take(self):
		clicks, self.clicks = self.clicks, []
		return self.pos, clicks


def find_port():
	for p in serial.tools.list_ports.comports():
		if p.vid == CH340_VID:
			return p.device
	return None


# ---------------- drawing ----------------
def crosshair_mask(cx, cy):			# same pixels as draw_crosshair() on the board
	m = np.zeros((H, W), bool)

	def p(x, y):
		if 0 <= x < W and 0 <= y < H:
			m[y, x] = True

	for i in range(-ARM, ARM + 1):
		p(cx + i, cy)
		p(cx, cy + i)
	x, y, err = RADIUS, 0, 1 - RADIUS
	while x >= y:
		for a, b in ((x, y), (-x, y), (x, -y), (-x, -y), (y, x), (-y, x), (y, -x), (-y, -x)):
			p(cx + a, cy + b)
		y += 1
		if err < 0:
			err += 2 * y + 1
		else:
			x -= 1
			err += 2 * (y - x) + 1
	return m


def render(pos, mouse, click_text):
	img = np.full((H, W, 3), (170, 215, 190), np.uint8)	# LCD green
	img[crosshair_mask(*pos)] = (40, 40, 40)
	img = cv2.resize(img, (W * SCALE, H * SCALE), interpolation=cv2.INTER_NEAREST)
	if mouse:						# small red dot = laptop mouse
		cv2.circle(img, (mouse[0] * SCALE + SCALE // 2, mouse[1] * SCALE + SCALE // 2), 3, (0, 0, 255), -1)
	cv2.putText(img, "cross hair X:%d Y:%d" % pos, (8, 22), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 0, 0), 1)
	if click_text:
		cv2.putText(img, click_text, (8, H * SCALE - 12), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 160), 2)
	return img


# ---------------- main ----------------
def main():
	ap = argparse.ArgumentParser(description="laptop mouse -> NUC140 cross hair over UART")
	ap.add_argument("--port", help="COM port of the CH340 (default: find it)")
	ap.add_argument("--baud", type=int, default=BAUD)
	ap.add_argument("--no-board", action="store_true", help="window only, no board")
	args = ap.parse_args()

	if args.no_board:
		board = NoBoard()
	else:
		port = args.port or find_port()
		if not port:
			raise SystemExit("CH340 not found, use --port COMx or --no-board")
		board = Board(port, args.baud)
		print("board on", port, args.baud)

	mouse = {"pos": None, "moved": False, "click": False}

	def on_mouse(event, x, y, flags, param):
		x = min(max(x // SCALE, 0), W - 1)
		y = min(max(y // SCALE, 0), H - 1)
		mouse["pos"] = (x, y)
		if event == cv2.EVENT_MOUSEMOVE:
			mouse["moved"] = True
		elif event == cv2.EVENT_LBUTTONDOWN:
			mouse["click"] = True

	cv2.namedWindow(WIN)
	cv2.setMouseCallback(WIN, on_mouse)

	last_send = 0.0
	click_text, click_until = "", 0.0
	while True:
		now = time.time()
		if mouse["click"]:				# a click is always sent at once
			board.send(*mouse["pos"], True)
			mouse["click"] = mouse["moved"] = False
			last_send = now
		elif mouse["moved"] and now - last_send >= SEND_GAP:
			board.send(*mouse["pos"], False)
			mouse["moved"] = False
			last_send = now

		pos, clicks = board.take()
		for x, y in clicks:
			print("click : X = %d  Y = %d" % (x, y))
			click_text, click_until = "click  X:%d  Y:%d" % (x, y), now + SHOW_TIME
		if now > click_until:
			click_text = ""

		cv2.imshow(WIN, render(pos, mouse["pos"], click_text))
		k = cv2.waitKey(10) & 0xFF
		if k in (ord("q"), 27) or cv2.getWindowProperty(WIN, cv2.WND_PROP_VISIBLE) < 1:
			break
	cv2.destroyAllWindows()


if __name__ == "__main__":
	main()
