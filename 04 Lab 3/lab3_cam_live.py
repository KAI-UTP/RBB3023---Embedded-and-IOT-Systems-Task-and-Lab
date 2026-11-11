#
# 1. Chan Li Kai
# 2. William Wong Xiao Kang
# 3. Irvin Chong Hou Ceng
# 4. Liang Yan Ee
#
# Lab 3 : live webcam on the Nu-LB-NUC140 LCD over UART
#         webcam -> OpenCV (face tracking, black/white) -> CH340 USB-TTL -> UART1 -> LCD
#
#   frame to board : 0xAA 0x55 + 1024 bytes (128x64, buffer[x + page*128], bit 1 = dark)
#   from board     : 'K' = frame shown, send the next one; '1'~'9' = keypad key
#
#   keys (board keypad or laptop) : 1 full view, 2 face zoom, 3 threshold, 4 dither,
#                                   5 edges, 6 invert, 7 face boxes, Q quit (laptop)
#
#   usage : py lab3_cam_live.py  [--port COM5]  [--no-board]
#
import argparse
import threading
import time

import cv2
import numpy as np
import serial
import serial.tools.list_ports

from lab3_video_to_lcd import pack_lcd, lcd_image, BAYER4, FACES, W, H

BAUD = 460800
CH340_VID = 0x1A86
BAYER_TILE = (np.tile(BAYER4, (H // 4, W // 4)) + 0.5) / 16 * 255
STYLES = {3: "threshold", 4: "dither", 5: "edges"}


class Board:
	def __init__(self, port, baud):
		self.ser = serial.Serial(port, baud, timeout=0.05)
		self.acks = 0
		self.keys = []
		self.lock = threading.Lock()
		threading.Thread(target=self.reader, daemon=True).start()

	def reader(self):					# background thread
		while True:
			try:
				data = self.ser.read(64)
			except serial.SerialException:
				return
			with self.lock:
				for c in data:
					if c == ord('K'):
						self.acks += 1
					elif ord('1') <= c <= ord('9'):
						self.keys.append(c - ord('0'))

	def send_frame(self, buf):
		self.ser.write(b"\xAA\x55" + buf)

	def take(self):					# acks since last call, keys pressed
		with self.lock:
			acks, keys = self.acks, self.keys
			self.acks, self.keys = 0, []
		return acks, keys


def find_port():
	for p in serial.tools.list_ports.comports():
		if p.vid == CH340_VID:
			return p.device
	return None


class FaceTracker:
	def __init__(self):
		self.box = None				# smoothed (cx, cy, size) of the biggest face
		self.seen = 0
		self.faces = []

	def update(self, gray):
		small = cv2.equalizeHist(cv2.resize(gray, (gray.shape[1] // 2, gray.shape[0] // 2)))
		found = FACES.detectMultiScale(small, 1.1, 4, minSize=(30, 30))
		self.faces = [(x * 2, y * 2, w * 2, h * 2) for x, y, w, h in found]
		if self.faces:
			x, y, w, h = max(self.faces, key=lambda f: f[2] * f[3])
			new = np.array([x + w / 2, y + h / 2, h], float)
			self.box = new if self.box is None else self.box * 0.7 + new * 0.3
			self.seen = time.time()


def crop_box(img_w, img_h, view, tracker):
	if view == "face" and tracker.box is not None and time.time() - tracker.seen < 1.5:
		cx, cy, size = tracker.box
		ch = int(min(img_h, max(64, size * 2.2)))	# face fills ~half the height
		cw = min(img_w, ch * 2)
		ch = cw // 2
	else:
		cx, cy = img_w / 2, img_h / 2
		cw = min(img_w, img_h * 2)
		ch = cw // 2
	x0 = int(np.clip(cx - cw / 2, 0, img_w - cw))
	y0 = int(np.clip(cy - ch / 2, 0, img_h - ch))
	return x0, y0, cw, ch


def to_lcd(gray, box, style, invert, faces, show_boxes):
	x0, y0, cw, ch = box
	g = cv2.resize(gray[y0:y0 + ch, x0:x0 + cw], (W, H), interpolation=cv2.INTER_AREA)
	g = cv2.createCLAHE(clipLimit=2.0, tileGridSize=(4, 4)).apply(g)

	if style == "dither":
		dark = g.astype(np.float32) < BAYER_TILE
	elif style == "edges":
		e = cv2.Canny(cv2.GaussianBlur(g, (3, 3), 0), 60, 140)
		dark = e > 0
	else:
		t = cv2.threshold(g, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)[0]
		dark = g < t
	if invert:
		dark = ~dark

	if show_boxes:
		sx, sy = W / cw, H / ch
		for x, y, w, h in faces:
			p0 = (int((x - x0) * sx), int((y - y0) * sy))
			p1 = (int((x + w - x0) * sx), int((y + h - y0) * sy))
			img = dark.astype(np.uint8)
			cv2.rectangle(img, p0, p1, 1 if not invert else 0, 1)
			dark = img.astype(bool)
	return dark


def main():
	ap = argparse.ArgumentParser(description="live webcam -> NUC140 LCD over UART")
	ap.add_argument("--port", help="COM port of the CH340 (default: find it)")
	ap.add_argument("--baud", type=int, default=BAUD)
	ap.add_argument("--cam", type=int, default=0)
	ap.add_argument("--no-board", action="store_true", help="laptop preview only")
	args = ap.parse_args()

	board = None
	if not args.no_board:
		port = args.port or find_port()
		if port is None:
			print("CH340 not found, preview only (use --port COMx if it has another name)")
		else:
			board = Board(port, args.baud)
			print("board on %s at %d baud" % (port, args.baud))

	cap = cv2.VideoCapture(args.cam, cv2.CAP_DSHOW)
	if not cap.isOpened():
		raise SystemExit("cannot open webcam %d" % args.cam)
	cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
	cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

	tracker = FaceTracker()
	view, style, invert, show_boxes = "face", "threshold", False, False
	last_send = 0.0
	waiting = False					# frame sent, no 'K' yet
	shown, fps, t_fps = 0, 0.0, time.time()

	print("keys : 1 full  2 face  3 threshold  4 dither  5 edges  6 invert  7 boxes  Q quit")
	while True:
		ok, img = cap.read()
		if not ok:
			continue
		img = cv2.flip(img, 1)				# mirror
		gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
		tracker.update(gray)

		box = crop_box(img.shape[1], img.shape[0], view, tracker)
		dark = to_lcd(gray, box, style, invert, tracker.faces, show_boxes)

		keys = []
		if board:
			acks, keys = board.take()
			if acks:
				waiting = False
				shown += acks
		k = cv2.waitKey(1) & 0xFF
		if k in (ord('q'), 27):
			break
		if ord('1') <= k <= ord('9'):
			keys.append(k - ord('0'))
		for key in keys:
			if key == 1:   view = "full"
			elif key == 2: view = "face"
			elif key in STYLES: style = STYLES[key]
			elif key == 6: invert = not invert
			elif key == 7: show_boxes = not show_boxes
			print("key %d -> view %s, style %s, invert %s, boxes %s" % (key, view, style, invert, show_boxes))

		# send when the board is ready ('K'), or after 0.3 s if a 'K' got lost
		if board and (not waiting or time.time() - last_send > 0.3):
			board.send_frame(pack_lcd(dark))
			last_send = time.time()
			waiting = True

		if time.time() - t_fps >= 1:
			fps = shown / (time.time() - t_fps)
			shown, t_fps = 0, time.time()

		# laptop window : camera + crop box, LCD preview, status bar
		cam = img.copy()
		x0, y0, cw, ch = box
		cv2.rectangle(cam, (x0, y0), (x0 + cw, y0 + ch), (0, 0, 255), 2)
		for x, y, w, h in tracker.faces:
			cv2.rectangle(cam, (x, y), (x + w, y + h), (0, 255, 0), 2)
		cam = cv2.resize(cam, (512, 384))
		lcd = cv2.cvtColor(lcd_image(dark, 4), cv2.COLOR_GRAY2BGR)
		bar = np.zeros((30, 512, 3), np.uint8)
		status = "%s | %s %s | board %s" % (view, style, "inv" if invert else "",
						    ("%.1f fps" % fps) if board else "not connected")
		cv2.putText(bar, status, (8, 21), cv2.FONT_HERSHEY_SIMPLEX, 0.55, (255, 255, 255), 1)
		cv2.imshow("Lab 3 : webcam -> NUC140 LCD", np.vstack([cam, lcd, bar]))

	cap.release()
	cv2.destroyAllWindows()


if __name__ == "__main__":
	main()
