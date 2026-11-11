#
# Task 2C (extra) : Balloon Shooter - laptop side for the MOUSE and FINGER modes
#         mouse / webcam -> OpenCV (+ MediaPipe hand) -> CH340 USB-TTL -> UART1 -> board
#
#   pick the mode on the board menu (keypad 1 / 2 / 3), this script follows it :
#     1 KEYPAD : nothing to do here, play on the board
#     2 MOUSE  : the window is the LCD drawn 6 times bigger, use it as a mouse pad
#                move = aim, left click = shoot
#     3 FINGER : the window is the webcam, point with your index finger
#                inside the green box = aim, pinch thumb + index = shoot
#   Q / ESC = quit
#
#   finger tracking : MediaPipe HandLandmarker (hand_landmarker.task, next to this file)
#     landmark 8 = index tip (aim), 4 = thumb tip, 0 = wrist, 5 = index knuckle
#     pinch = (thumb tip to index tip) / (wrist to index knuckle) < 0.30,
#             released again above 0.45 (no double shots)
#     the shot uses the aim from 3 frames before the pinch, because pinching
#     moves the index tip a little
#
#   to board   : 0xAA, x (0~127), y (0~63), flag (0 = move, 1 = click / pinch, 2 = no hand)
#                sent at least every 0.3 s so the board knows the laptop is there
#   from board : "P x y\n" cross hair, "S score lives\n", "M m\n" mode (0 = menu)
#
#   usage :
#     py task2c_control.py                  find the CH340 port by itself
#     py task2c_control.py --port COM5      choose the port
#     py task2c_control.py --cam 1          use another webcam
#
import argparse
import collections
import os
import threading
import time

import cv2
import mediapipe as mp
import numpy as np
import serial
import serial.tools.list_ports
from mediapipe.tasks.python import BaseOptions, vision

W, H = 128, 64
SCALE = 6
RADIUS, ARM = 6, 9			# same cross hair as the board game
BAUD = 115200
CH340_VID = 0x1A86
SEND_GAP = 0.02				# send at most one move every 20 ms
ALIVE_GAP = 0.3				# send something at least every 0.3 s
BOX = (0.20, 0.20, 0.80, 0.75)		# finger area in the camera (x0, y0, x1, y1), 0~1
PINCH_ON, PINCH_OFF = 0.30, 0.45
SMOOTH = 0.5				# finger aim smoothing, 1 = none
HERE = os.path.dirname(os.path.abspath(__file__))
MODEL = os.path.join(HERE, "hand_landmarker.task")
WIN = "Task 2C : Balloon Shooter"
MODE_NAME = {None: "waiting for board", 0: "MENU", 1: "KEYPAD", 2: "MOUSE", 3: "FINGER"}


# ---------------- board link ----------------
class Board:
	def __init__(self, port, baud):
		self.ser = serial.Serial(port, baud, timeout=0.05)
		self.pos = (W // 2, 36)			# cross hair position from the board
		self.score, self.lives = 0, 3
		self.mode = None			# not heard yet
		self.last_send = 0.0
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
				if not parts or not all(p.isdigit() for p in parts[1:]):
					continue
				nums = [int(p) for p in parts[1:]]
				with self.lock:
					if parts[0] == "P" and len(nums) == 2:
						self.pos = tuple(nums)
					elif parts[0] == "S" and len(nums) == 2:
						self.score, self.lives = nums
					elif parts[0] == "M" and len(nums) == 1:
						if nums[0] != self.mode:
							print("board mode :", MODE_NAME.get(nums[0], nums[0]))
						self.mode = nums[0]

	def send(self, x, y, flag):
		self.ser.write(bytes([0xAA, int(x), int(y), flag]))
		self.last_send = time.time()

	def state(self):
		with self.lock:
			return self.mode, self.pos, self.score, self.lives


def find_port():
	for p in serial.tools.list_ports.comports():
		if p.vid == CH340_VID:
			return p.device
	return None


# ---------------- drawing ----------------
def crosshair_mask(cx, cy):			# same pixels as the board cross hair
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


def text(img, s, org, size=0.6, color=(0, 0, 0), thick=1):
	cv2.putText(img, s, org, cv2.FONT_HERSHEY_SIMPLEX, size, color, thick, cv2.LINE_AA)


def render_pad(pos, score, lives, mouse):		# MOUSE mode window
	img = np.full((H, W, 3), (170, 215, 190), np.uint8)	# LCD green
	img[crosshair_mask(*pos)] = (40, 40, 40)
	img = cv2.resize(img, (W * SCALE, H * SCALE), interpolation=cv2.INTER_NEAREST)
	if mouse:						# small red dot = laptop mouse
		cv2.circle(img, (mouse[0] * SCALE + SCALE // 2, mouse[1] * SCALE + SCALE // 2), 3, (0, 0, 255), -1)
	text(img, "MOUSE   SCORE %d   LIVES %d" % (score, lives), (8, 24), 0.7, thick=2)
	text(img, "move = aim   click = shoot   Q = quit", (8, H * SCALE - 12), 0.5, (60, 60, 60))
	return img


def render_info(mode, score, lives):			# MENU / KEYPAD / no board yet
	img = np.full((H * SCALE, W * SCALE, 3), 235, np.uint8)
	text(img, "Board mode : " + MODE_NAME.get(mode, str(mode)), (20, 60), 0.9, thick=2)
	if mode == 1:
		text(img, "Keypad mode : play on the board", (20, 120), 0.7)
		text(img, "SCORE %d   LIVES %d" % (score, lives), (20, 160), 0.7)
	else:
		text(img, "Pick the mode on the board keypad :", (20, 120), 0.7)
		text(img, "1 = KEYPAD    2 = MOUSE    3 = FINGER", (20, 160), 0.7)
	text(img, "Q = quit", (20, H * SCALE - 20), 0.5, (90, 90, 90))
	return img


# ---------------- finger tracking ----------------
class Finger:
	def __init__(self, cam):
		if not os.path.exists(MODEL):
			raise SystemExit("hand_landmarker.task not found next to this script")
		opts = vision.HandLandmarkerOptions(
			base_options=BaseOptions(model_asset_path=MODEL),
			running_mode=vision.RunningMode.VIDEO, num_hands=1,
			min_hand_detection_confidence=0.5, min_tracking_confidence=0.5)
		self.hands = vision.HandLandmarker.create_from_options(opts)
		self.cap = cv2.VideoCapture(cam, cv2.CAP_DSHOW)
		if not self.cap.isOpened():
			raise SystemExit("cannot open webcam %d" % cam)
		self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
		self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
		self.t_ms = 0
		self.aim = None				# smoothed LCD position (float)
		self.history = collections.deque(maxlen=6)
		self.pinched = False

	def close(self):
		self.cap.release()
		self.hands.close()

	# one camera frame -> (image, aim or None, shot position or None)
	def step(self):
		ok, img = self.cap.read()
		if not ok:
			return None, None, None
		img = cv2.flip(img, 1)				# mirror, like a mirror
		h, w = img.shape[:2]
		self.t_ms = max(self.t_ms + 1, int(time.monotonic() * 1000))
		rgb = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
		res = self.hands.detect_for_video(mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb), self.t_ms)

		x0, y0, x1, y1 = BOX
		cv2.rectangle(img, (int(x0 * w), int(y0 * h)), (int(x1 * w), int(y1 * h)), (0, 200, 0), 2)
		if not res.hand_landmarks:
			self.aim, self.pinched = None, False
			self.history.clear()
			text(img, "show your hand", (10, 30), 0.8, (0, 0, 255), 2)
			return img, None, None

		lm = res.hand_landmarks[0]
		pt = lambda i: np.array([lm[i].x * w, lm[i].y * h])

		# aim : index tip inside the green box -> LCD 128 x 64
		tx = (lm[8].x - x0) / (x1 - x0) * (W - 1)
		ty = (lm[8].y - y0) / (y1 - y0) * (H - 1)
		target = np.clip([tx, ty], [0, 0], [W - 1, H - 1])
		self.aim = target if self.aim is None else self.aim + SMOOTH * (target - self.aim)
		aim = (int(round(self.aim[0])), int(round(self.aim[1])))
		self.history.append(aim)

		# pinch : thumb tip close to index tip, compared with the hand size
		size = np.linalg.norm(pt(0) - pt(5)) + 1e-6
		ratio = np.linalg.norm(pt(4) - pt(8)) / size
		shot = None
		if not self.pinched and ratio < PINCH_ON:
			self.pinched = True
			shot = self.history[-4] if len(self.history) >= 4 else aim
		elif self.pinched and ratio > PINCH_OFF:
			self.pinched = False

		color = (0, 0, 255) if self.pinched else (255, 120, 0)
		cv2.line(img, tuple(pt(4).astype(int)), tuple(pt(8).astype(int)), color, 2)
		cv2.circle(img, tuple(pt(8).astype(int)), 8, color, -1)
		cv2.circle(img, tuple(pt(4).astype(int)), 6, color, -1)
		text(img, "PINCH" if self.pinched else "aim  (pinch = shoot)", (10, 30), 0.8, color, 2)
		return img, aim, shot


def render_finger(img, pos, score, lives):		# FINGER mode window
	h, w = img.shape[:2]
	x0, y0, x1, y1 = BOX					# board cross hair drawn back into the box
	px = int((x0 + pos[0] / (W - 1) * (x1 - x0)) * w)
	py = int((y0 + pos[1] / (H - 1) * (y1 - y0)) * h)
	cv2.circle(img, (px, py), 18, (0, 0, 0), 2)
	cv2.line(img, (px - 26, py), (px + 26, py), (0, 0, 0), 2)
	cv2.line(img, (px, py - 26), (px, py + 26), (0, 0, 0), 2)
	text(img, "FINGER   SCORE %d   LIVES %d" % (score, lives), (10, h - 15), 0.7, (0, 0, 0), 2)
	return img


# ---------------- main ----------------
def main():
	ap = argparse.ArgumentParser(description="mouse / finger control for the NUC140 Balloon Shooter")
	ap.add_argument("--port", help="COM port of the CH340 (default: find it)")
	ap.add_argument("--baud", type=int, default=BAUD)
	ap.add_argument("--cam", type=int, default=0)
	args = ap.parse_args()

	port = args.port or find_port()
	if not port:
		raise SystemExit("CH340 not found, use --port COMx")
	board = Board(port, args.baud)
	print("board on", port, args.baud, "- pick the mode on the board keypad")

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

	finger = None
	last_move = 0.0
	try:
		while True:
			mode, pos, score, lives = board.state()
			now = time.time()

			if mode != 3 and finger:			# left finger mode : camera off
				finger.close()
				finger = None

			if mode == 2:					# ---- MOUSE ----
				if mouse["click"] and mouse["pos"]:
					board.send(*mouse["pos"], 1)
					mouse["click"] = mouse["moved"] = False
				elif mouse["moved"] and now - last_move >= SEND_GAP:
					board.send(*mouse["pos"], 0)
					mouse["moved"] = False
					last_move = now
				elif now - board.last_send >= ALIVE_GAP:
					board.send(*(mouse["pos"] or pos), 0)
				img = render_pad(pos, score, lives, mouse["pos"])

			elif mode == 3:					# ---- FINGER ----
				if finger is None:
					finger = Finger(args.cam)
				img, aim, shot = finger.step()
				if img is None:
					continue
				if shot:
					board.send(*shot, 1)
					print("pinch : shoot at X = %d  Y = %d" % shot)
				elif aim:
					if now - last_move >= SEND_GAP:
						board.send(*aim, 0)
						last_move = now
				elif now - board.last_send >= 0.1:
					board.send(0, 0, 2)		# no hand
				img = render_finger(img, pos, score, lives)

			else:						# ---- MENU / KEYPAD / not heard yet ----
				mouse["click"] = mouse["moved"] = False
				img = render_info(mode, score, lives)

			cv2.imshow(WIN, img)
			k = cv2.waitKey(10) & 0xFF
			if k in (ord("q"), 27) or cv2.getWindowProperty(WIN, cv2.WND_PROP_VISIBLE) < 1:
				break
	finally:
		if finger:
			finger.close()
		cv2.destroyAllWindows()


if __name__ == "__main__":
	main()
