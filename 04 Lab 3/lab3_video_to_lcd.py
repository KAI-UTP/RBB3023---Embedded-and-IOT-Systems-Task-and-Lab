#
# 1. Chan Li Kai
# 2. William Wong Xiao Kang
# 3. Irvin Chong Hou Ceng
# 4. Liang Yan Ee
#
# Lab 3 : turn a video (mp4 or webcam) into video_data.c, played on the LCD by main_video_backup.c
#
#   LCD frame : 128x64, buffer[x + page*128], bit (y & 7), bit 1 = dark pixel
#   half mode (default) : 64x32 frames drawn as 2x2 blocks (Keil MDK-Lite links only up to 32 KB)
#   --full    : 128x64, needs a Keil licence without the 32 KB limit
#   crop      : 2:1 window that follows the biggest face (OpenCV Haar cascade)
#   compression : XOR with the frame before, then PackBits
#                 n = 0~127 : copy next n+1 bytes, n = 129~255 : repeat next byte 257-n times
#
#   usage : py lab3_video_to_lcd.py --video clip.mp4   (or --seconds 10 to record from webcam)
#
import argparse
import os
import time

import cv2
import numpy as np

W, H = 128, 64
HERE = os.path.dirname(os.path.abspath(__file__))

BAYER4 = np.array([[0, 8, 2, 10],
                   [12, 4, 14, 6],
                   [3, 11, 1, 9],
                   [15, 7, 13, 5]], dtype=np.float32)

FACES = cv2.CascadeClassifier(cv2.data.haarcascades + "haarcascade_frontalface_default.xml")


def record_webcam(seconds, cam, save_path):
	cap = cv2.VideoCapture(cam, cv2.CAP_DSHOW)
	if not cap.isOpened():
		raise SystemExit("cannot open webcam %d" % cam)

	t0 = time.time()					# 3 s countdown
	while True:
		ok, img = cap.read()
		if not ok:
			continue
		img = cv2.flip(img, 1)
		left = 3 - int(time.time() - t0)
		show = img.copy()
		cv2.putText(show, "Recording in %d  (SPACE = now)" % left, (10, 30),
			    cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 0, 255), 2)
		cv2.imshow("Lab 3 webcam", show)
		key = cv2.waitKey(1) & 0xFF
		if key in (ord('q'), 27):
			raise SystemExit("cancelled")
		if left <= 0 or key == ord(' '):
			break

	frames = []
	t0 = time.time()
	while time.time() - t0 < seconds:
		ok, img = cap.read()
		if not ok:
			continue
		img = cv2.flip(img, 1)
		frames.append(img)
		show = img.copy()
		cv2.putText(show, "REC %.1f s  (Q = stop)" % (time.time() - t0), (10, 30),
			    cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 0, 255), 2)
		cv2.imshow("Lab 3 webcam", show)
		if (cv2.waitKey(1) & 0xFF) in (ord('q'), 27):
			break
	cap.release()
	cv2.destroyWindow("Lab 3 webcam")
	if not frames:
		raise SystemExit("no frames recorded")

	src_fps = len(frames) / max(time.time() - t0, 0.1)
	h, w = frames[0].shape[:2]
	out = cv2.VideoWriter(save_path, cv2.VideoWriter_fourcc(*"mp4v"), src_fps, (w, h))
	for f in frames:
		out.write(f)
	out.release()
	print("recorded %d frames (%.1f fps) -> %s" % (len(frames), src_fps, save_path))
	return frames, src_fps


def load_video(path):
	cap = cv2.VideoCapture(path)
	if not cap.isOpened():
		raise SystemExit("cannot open " + path)
	src_fps = cap.get(cv2.CAP_PROP_FPS) or 30
	frames = []
	while True:
		ok, img = cap.read()
		if not ok:
			break
		frames.append(img)
	cap.release()
	if not frames:
		raise SystemExit("no frames in " + path)
	print("loaded %d frames (%.1f fps, %dx%d) from %s"
	      % (len(frames), src_fps, frames[0].shape[1], frames[0].shape[0], os.path.basename(path)))
	return frames, src_fps


# 2:1 crop window per frame, following the biggest face
def crop_windows(frames):
	h, w = frames[0].shape[:2]
	cw, ch = (w, w // 2) if w <= 2 * h else (2 * h, h)
	found_in = []					# (cx, cy, area) or None
	for img in frames:
		gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
		small = cv2.equalizeHist(cv2.resize(gray, (w // 2, h // 2)))
		found = FACES.detectMultiScale(small, 1.1, 3, minSize=(30, 30))
		if len(found):
			x, y, fw, fh = max(found, key=lambda f: f[2] * f[3])
			found_in.append(((x + fw / 2) * 2, (y + fh / 2) * 2, fw * fh))
		else:
			found_in.append(None)

	# the detector often misses the nearest face, so each frame uses the
	# biggest face found within +-15 frames (about 0.5 s), else the last one
	cx, cy, last = [], [], (w / 2, h / 2)
	for i in range(len(frames)):
		near = [f for f in found_in[max(0, i - 15):i + 16] if f is not None]
		if near:
			last = max(near, key=lambda f: f[2])[:2]
		cx.append(last[0])
		cy.append(last[1])
	k = np.ones(7) / 7
	cx = np.convolve(np.pad(cx, 3, mode="edge"), k, mode="valid")
	cy = np.convolve(np.pad(cy, 3, mode="edge"), k, mode="valid")

	boxes = []
	for x, y in zip(cx, cy):
		x0 = int(np.clip(x - cw / 2, 0, w - cw))
		y0 = int(np.clip(y - ch / 2, 0, h - ch))
		boxes.append((x0, y0, cw, ch))
	print("faces found in %d of %d frames" % (sum(f is not None for f in found_in), len(frames)))
	return boxes


def to_bw(img, box, dither, invert, fw, fh):
	x0, y0, cw, ch = box
	gray = cv2.cvtColor(img[y0:y0 + ch, x0:x0 + cw], cv2.COLOR_BGR2GRAY)
	gray = cv2.resize(gray, (fw, fh), interpolation=cv2.INTER_AREA)
	gray = cv2.createCLAHE(clipLimit=2.0, tileGridSize=(4, 4)).apply(gray)

	if dither == "bayer":
		tile = (np.tile(BAYER4, (fh // 4, fw // 4)) + 0.5) / 16 * 255
		dark = gray.astype(np.float32) < tile
	elif dither == "fs":					# Floyd-Steinberg
		g = gray.astype(np.float32)
		dark = np.zeros((fh, fw), bool)
		for y in range(fh):
			for x in range(fw):
				old = g[y, x]
				new = 255.0 if old >= 128 else 0.0
				dark[y, x] = new == 0
				e = old - new
				if x + 1 < fw:
					g[y, x + 1] += e * 7 / 16
				if y + 1 < fh:
					if x > 0:
						g[y + 1, x - 1] += e * 3 / 16
					g[y + 1, x] += e * 5 / 16
					if x + 1 < fw:
						g[y + 1, x + 1] += e / 16
	else:
		t = cv2.threshold(gray, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)[0]
		dark = gray < t
	return ~dark if invert else dark


# fw x fh bool (True = dark) -> bytes in LCD page format, buffer[x + page*fw]
def pack_lcd(dark):
	fh, fw = dark.shape
	pages = dark.reshape(fh // 8, 8, fw).astype(np.uint8)
	buf = np.zeros((fh // 8, fw), np.uint8)
	for b in range(8):
		buf |= pages[:, b, :] << b
	return buf.tobytes()


def packbits(data):
	out = bytearray()
	i, n = 0, len(data)
	while i < n:
		run = 1
		while i + run < n and run < 128 and data[i + run] == data[i]:
			run += 1
		if run >= 2:
			out += bytes([257 - run, data[i]])
			i += run
		else:
			j = i
			while j < n and j - i < 128:
				if j + 1 < n and data[j] == data[j + 1]:
					break
				j += 1
			out += bytes([j - i - 1]) + data[i:j]
			i = j
	return bytes(out)


def unpackbits(data, size):			# same as the C decoder, used to check
	out = bytearray()
	i = 0
	while len(out) < size:
		c = data[i]
		i += 1
		if c < 128:
			out += data[i:i + c + 1]
			i += c + 1
		else:
			out += bytes([data[i]]) * (257 - c)
			i += 1
	assert len(out) == size
	return bytes(out), i


def encode(lcd_frames):
	stream = bytearray()
	prev = np.zeros(len(lcd_frames[0]), np.uint8)	# board starts with a blank screen
	for f in lcd_frames:
		cur = np.frombuffer(f, np.uint8)
		stream += packbits((cur ^ prev).tobytes())
		prev = cur
	return bytes(stream)


def verify(stream, lcd_frames):
	size = len(lcd_frames[0])
	screen = np.zeros(size, np.uint8)
	pos = 0
	for f in lcd_frames:
		delta, used = unpackbits(stream[pos:pos + 3 * size], size)
		pos += used
		screen ^= np.frombuffer(delta, np.uint8)
		assert screen.tobytes() == f, "decode check failed"
	assert pos == len(stream)


NAMES = ("//\n// 1. Chan Li Kai\n// 2. William Wong Xiao Kang\n"
	 "// 3. Irvin Chong Hou Ceng\n// 4. Liang Yan Ee\n//\n")


def write_c(stream, count, fps, src, half):
	with open(os.path.join(HERE, "video_data.h"), "w") as f:
		f.write(NAMES + "// video_data.h : made by lab3_video_to_lcd.py, do not edit\n//\n")
		f.write("#ifndef _VIDEO_DATA_H\n#define _VIDEO_DATA_H\n\n#include <stdint.h>\n\n")
		f.write("#define VIDEO_HALF\t%d\t\t// 1 = 64x32 drawn as 2x2 blocks, 0 = 128x64\n" % half)
		f.write("#define VIDEO_W\t\t%d\n#define VIDEO_H\t\t%d\n" % ((64, 32) if half else (128, 64)))
		f.write("#define VIDEO_FRAMES\t%d\t\t// number of frames\n" % count)
		f.write("#define VIDEO_FRAME_US\t%d\t// time per frame (us), %.0f fps\n" % (int(1000000 / fps), fps))
		f.write("#define VIDEO_BYTES\t%d\t// size of VIDEO[]\n\n" % len(stream))
		f.write("extern const uint8_t VIDEO[VIDEO_BYTES];\n\n#endif\n")
	with open(os.path.join(HERE, "video_data.c"), "w") as f:
		f.write(NAMES + "// video_data.c : made by lab3_video_to_lcd.py, do not edit\n")
		f.write("//   source : %s\n" % src)
		f.write("//   %d frames, %.0f fps, %s, %d bytes (XOR delta + PackBits)\n//\n"
			% (count, fps, "64x32 x2" if half else "128x64", len(stream)))
		f.write('#include "video_data.h"\n\nconst uint8_t VIDEO[VIDEO_BYTES] = {\n')
		for i in range(0, len(stream), 16):
			f.write("\t" + ", ".join("0x%02X" % b for b in stream[i:i + 16]) + ",\n")
		f.write("};\n")


# LCD picture enlarged for the laptop screen
def lcd_image(dark, scale=4):
	img = np.where(dark, 30, 190).astype(np.uint8)
	return cv2.resize(img, (W * scale, H * scale), interpolation=cv2.INTER_NEAREST)


def preview(bw, fps):
	print("preview : Q / ESC to close")
	delay = max(1, int(1000 / fps))
	while True:
		for dark in bw:
			cv2.imshow("LCD preview 128x64", lcd_image(dark, 5))
			if (cv2.waitKey(delay) & 0xFF) in (ord('q'), 27):
				cv2.destroyAllWindows()
				return


def convert(frames, boxes, src_fps, fps, args):
	fw, fh = (W, H) if args.full else (W // 2, H // 2)
	step = src_fps / fps
	idx = [int(i * step) for i in range(int(len(frames) / step))]
	return [to_bw(frames[i], boxes[i], args.dither, args.invert, fw, fh) for i in idx]


def main():
	ap = argparse.ArgumentParser(description="video / webcam -> NUC140 LCD animation")
	ap.add_argument("--video", help="video file (else record from webcam)")
	ap.add_argument("--seconds", type=float, default=10, help="webcam recording length")
	ap.add_argument("--cam", type=int, default=0, help="webcam number")
	ap.add_argument("--fps", type=float, default=12, help="animation frames per second")
	ap.add_argument("--dither", choices=["bayer", "fs", "threshold"], default="threshold")
	ap.add_argument("--invert", action="store_true", help="swap black and white")
	ap.add_argument("--full", action="store_true", help="128x64 frames (needs Keil without the 32 KB limit)")
	ap.add_argument("--budget", type=int, help="max flash bytes for the video "
			"(default 20500 = fits Keil MDK-Lite 32 KB, 100000 with --full)")
	ap.add_argument("--no-preview", action="store_true")
	args = ap.parse_args()
	if args.budget is None:
		args.budget = 100000 if args.full else 20500

	if args.video:
		frames, src_fps = load_video(args.video)
		src = os.path.basename(args.video)
	else:
		frames, src_fps = record_webcam(args.seconds, args.cam, os.path.join(HERE, "lab3_webcam.mp4"))
		src = "webcam, %.1f s" % args.seconds

	boxes = crop_windows(frames)
	fps = min(args.fps, src_fps)
	bw = convert(frames, boxes, src_fps, fps, args)

	# lower the fps, then cut the end, until it fits in flash
	while True:
		lcd = [pack_lcd(d) for d in bw]
		stream = encode(lcd)
		if len(stream) <= args.budget:
			break
		if fps > 6:
			print("too big (%d bytes) at %.0f fps, trying %.0f fps" % (len(stream), fps, fps - 2))
			fps -= 2
			bw = convert(frames, boxes, src_fps, fps, args)
		else:
			keep = int(len(bw) * args.budget / len(stream) * 0.95)
			print("too big (%d bytes), cutting to %d frames" % (len(stream), keep))
			bw = bw[:keep]

	verify(stream, lcd)
	write_c(stream, len(lcd), fps, src, 0 if args.full else 1)
	raw = len(lcd) * len(lcd[0])
	print("video_data.c : %d frames, %.0f fps, %.1f s, %d bytes (raw %d, %.1fx smaller)"
	      % (len(lcd), fps, len(lcd) / fps, len(stream), raw, raw / len(stream)))

	if not args.no_preview:
		preview(bw, fps)


if __name__ == "__main__":
	main()
