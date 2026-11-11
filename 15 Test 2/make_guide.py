# make_guide.py : builds LPC1768_Complete_Guide.pdf (Test 2 study guide)
#   run:  py make_guide.py
#   needs the images in IMG (screenshots, LCD renders, diagrams) and the
#   example programs in Guide_Examples/ (all compiled with 0 errors)
import os, re
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.enums import TA_CENTER
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (BaseDocTemplate, PageTemplate, Frame, Paragraph, Spacer, Image,
                                Table, TableStyle, PageBreak, XPreformatted, KeepTogether, CondPageBreak)
from reportlab.platypus.tableofcontents import TableOfContents
from PIL import Image as PILImage

HERE = os.path.dirname(os.path.abspath(__file__))
EX = os.path.join(HERE, "Guide_Examples")
IMG = os.environ.get("GUIDE_IMG", os.path.join(HERE, "Guide_Images"))
OUT = os.path.join(HERE, "LPC1768_Complete_Guide.pdf")

F = "C:/Windows/Fonts/"
pdfmetrics.registerFont(TTFont("UI", F + "segoeui.ttf"))
pdfmetrics.registerFont(TTFont("UI-B", F + "segoeuib.ttf"))
pdfmetrics.registerFont(TTFont("UI-I", F + "segoeuii.ttf"))
pdfmetrics.registerFont(TTFont("UI-BI", F + "segoeuiz.ttf"))
pdfmetrics.registerFont(TTFont("Mono", F + "consola.ttf"))
pdfmetrics.registerFont(TTFont("Mono-B", F + "consolab.ttf"))
from reportlab.lib.fonts import addMapping
addMapping("UI", 0, 0, "UI"); addMapping("UI", 1, 0, "UI-B"); addMapping("UI", 0, 1, "UI-I"); addMapping("UI", 1, 1, "UI-BI")
addMapping("Mono", 0, 0, "Mono"); addMapping("Mono", 1, 0, "Mono-B"); addMapping("Mono", 0, 1, "Mono"); addMapping("Mono", 1, 1, "Mono-B")

NAVY = colors.HexColor("#1c345c"); BLUE = colors.HexColor("#2463b4"); GREY = colors.HexColor("#5b6470")
LIGHT = colors.HexColor("#f3f6fa"); LINE = colors.HexColor("#d5dbe3")
W, H = A4; M = 18 * mm; TW = W - 2 * M

sty = {
	"title": ParagraphStyle("title", fontName="UI-B", fontSize=30, leading=36, textColor=NAVY, alignment=TA_CENTER),
	"sub": ParagraphStyle("sub", fontName="UI", fontSize=14, leading=20, textColor=GREY, alignment=TA_CENTER),
	"h1": ParagraphStyle("h1", fontName="UI-B", fontSize=19, leading=24, textColor=NAVY, spaceBefore=4, spaceAfter=8),
	"h2": ParagraphStyle("h2", fontName="UI-B", fontSize=13.5, leading=18, textColor=BLUE, spaceBefore=10, spaceAfter=4),
	"h3": ParagraphStyle("h3", fontName="UI-B", fontSize=11, leading=15, textColor=NAVY, spaceBefore=6, spaceAfter=2),
	"p": ParagraphStyle("p", fontName="UI", fontSize=10, leading=14.2, spaceAfter=5),
	"li": ParagraphStyle("li", fontName="UI", fontSize=10, leading=14, leftIndent=14, bulletIndent=3, spaceAfter=2),
	"cap": ParagraphStyle("cap", fontName="UI-I", fontSize=8.8, leading=11.5, textColor=GREY, alignment=TA_CENTER, spaceAfter=8),
	"cell": ParagraphStyle("cell", fontName="UI", fontSize=8.8, leading=11.4),
	"cellb": ParagraphStyle("cellb", fontName="UI-B", fontSize=8.8, leading=11.4, textColor=colors.white),
	"code": ParagraphStyle("code", fontName="Mono", fontSize=7.9, leading=9.9, backColor=LIGHT,
	                       borderColor=LINE, borderWidth=0.6, borderPadding=(5, 6, 5, 6), spaceBefore=6, spaceAfter=10),
	"toc1": ParagraphStyle("toc1", fontName="UI-B", fontSize=10.5, leading=15, leftIndent=0),
	"toc2": ParagraphStyle("toc2", fontName="UI", fontSize=9.5, leading=12.5, leftIndent=16, textColor=GREY),
}

story = []
def P(t, s="p"): story.append(Paragraph(t, sty[s]))
def H1(t):
	story.append(PageBreak()); p = Paragraph(t, sty["h1"]); p._toc = (0, t); story.append(p)
def H2(t):
	story.append(CondPageBreak(40 * mm)); p = Paragraph(t, sty["h2"]); p._toc = (1, t); story.append(p)
def H3(t): story.append(Paragraph(t, sty["h3"]))
def UL(items):
	for i in items: story.append(Paragraph(i, sty["li"], bulletText="\u2022"))
def OL(items):
	for n, i in enumerate(items, 1): story.append(Paragraph(i, sty["li"], bulletText="%d." % n))
def SP(h=4): story.append(Spacer(1, h))
def c(t): return "<font name='Mono' color='#9c2c50'>%s</font>" % t.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")

def TABLE(rows, widths, head=True, zebra=True, fs=None):
	data = []
	for r, row in enumerate(rows):
		data.append([Paragraph(str(x), sty["cellb"] if (head and r == 0) else sty["cell"]) for x in row])
	t = Table(data, colWidths=[w * TW for w in widths], repeatRows=1 if head else 0)
	st = [("GRID", (0, 0), (-1, -1), 0.5, LINE), ("VALIGN", (0, 0), (-1, -1), "TOP"),
	      ("TOPPADDING", (0, 0), (-1, -1), 3), ("BOTTOMPADDING", (0, 0), (-1, -1), 3)]
	if head: st.append(("BACKGROUND", (0, 0), (-1, 0), NAVY))
	if zebra:
		for r in range(1 if head else 0, len(rows)):
			if r % 2 == 0: st.append(("BACKGROUND", (0, r), (-1, r), LIGHT))
	t.setStyle(TableStyle(st)); story.append(t); SP(8)

def BOX(title, text, kind="tip"):
	col = {"tip": "#1f8a4c", "warn": "#c0392b", "info": "#2463b4"}[kind]
	bg = {"tip": "#ecf8f0", "warn": "#fdecea", "info": "#eaf1fb"}[kind]
	body = [Paragraph("<font name='UI-B' color='%s'>%s</font>" % (col, title), sty["p"])]
	for t in (text if isinstance(text, list) else [text]):
		body.append(Paragraph(t, ParagraphStyle("bx", parent=sty["p"], spaceAfter=2)))
	t = Table([[body]], colWidths=[TW])
	t.setStyle(TableStyle([("BACKGROUND", (0, 0), (-1, -1), colors.HexColor(bg)),
	                       ("LINEBEFORE", (0, 0), (0, -1), 3.5, colors.HexColor(col)),
	                       ("LEFTPADDING", (0, 0), (-1, -1), 10), ("TOPPADDING", (0, 0), (-1, -1), 6),
	                       ("BOTTOMPADDING", (0, 0), (-1, -1), 6)]))
	story.append(t); SP(8)

def IMGF(name, width_frac=1.0, cap=None, max_h=None):
	path = os.path.join(IMG, name)
	w0, h0 = PILImage.open(path).size
	w = TW * width_frac; h = w * h0 / w0
	if max_h and h > max_h: h = max_h; w = h * w0 / h0
	items = [Image(path, width=w, height=h)]
	if cap: items.append(Paragraph(cap, sty["cap"]))
	else: items.append(Spacer(1, 8))
	story.append(KeepTogether(items))

KW = r"\b(int|void|bool|char|float|double|uint8_t|uint16_t|uint32_t|int16_t|int32_t|volatile|const|static|unsigned|return|if|else|while|for|true|false|break|continue|switch|case|default|struct|class|public|private|sizeof)\b"
TY = r"\b(DigitalOut|DigitalIn|InterruptIn|AnalogIn|AnalogOut|PwmOut|Timer|Ticker|Timeout|BufferedSerial|UnbufferedSerial|I2C|SPI|LCD_C12832|ThisThread|PullDown|PullUp|PullNone)\b"
def hl_code(code):
	out = []
	for ln in code.split("\n"):
		ln = ln.expandtabs(8)
		cm = ""
		m = re.search(r"//.*$", ln)
		if m and ln.count('"', 0, m.start()) % 2 == 0:
			cm = ln[m.start():]; ln = ln[:m.start()]
		def esc(s): return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
		parts = re.split(r'("(?:[^"\\]|\\.)*"|\'(?:[^\'\\]|\\.)*\')', ln)
		seg = []
		for i, p in enumerate(parts):
			if i % 2:
				seg.append("<font color='#a3410f'>%s</font>" % esc(p))
			else:
				p = esc(p)
				if p.lstrip().startswith("#"):
					seg.append("<font color='#7a3e9d'>%s</font>" % p); continue
				p = re.sub(KW, r"<font color='#0b4fb0'><b>\1</b></font>", p)
				p = re.sub(TY, r"<font color='#127a72'><b>\1</b></font>", p)
				seg.append(p)
		s = "".join(seg)
		if cm: s += "<font color='#3d7a3d'>%s</font>" % esc(cm)
		out.append(s)
	return "\n".join(out)

def CODE(text): story.append(XPreformatted(hl_code(text.strip("\n")), sty["code"]))
def CODEFILE(name):
	CODE(open(os.path.join(EX, name), encoding="utf-8").read())

# ------------------------------------------------------------------ page template
class Doc(BaseDocTemplate):
	def __init__(self, fn, **kw):
		BaseDocTemplate.__init__(self, fn, pagesize=A4, leftMargin=M, rightMargin=M, topMargin=20 * mm, bottomMargin=17 * mm, **kw)
		fr = Frame(M, 17 * mm, TW, H - 37 * mm, id="f", leftPadding=0, rightPadding=0, topPadding=0, bottomPadding=0)
		self.addPageTemplates([PageTemplate("cover", [fr], onPage=lambda cv, d: None),
		                       PageTemplate("main", [fr], onPage=self.deco)])
	def deco(self, cv, d):
		cv.saveState()
		cv.setFillColor(NAVY); cv.rect(0, H - 11 * mm, W, 11 * mm, fill=1, stroke=0)
		cv.setFillColor(colors.white); cv.setFont("UI-B", 9)
		cv.drawString(M, H - 7.2 * mm, "LPC1768 + mbed Application Board  |  Complete Guide")
		cv.setFont("UI", 9); cv.drawRightString(W - M, H - 7.2 * mm, "RBB3023 Embedded & IoT Systems  -  Test 2")
		cv.setStrokeColor(LINE); cv.line(M, 12 * mm, W - M, 12 * mm)
		cv.setFillColor(GREY); cv.setFont("UI", 8.5)
		cv.drawString(M, 8 * mm, "Mbed OS 6  |  Mbed Studio  |  all example code compiled with 0 errors")
		cv.drawRightString(W - M, 8 * mm, "Page %d" % d.page)
		cv.restoreState()
	def afterFlowable(self, f):
		if hasattr(f, "_toc"):
			lvl, text = f._toc
			key = "h%d" % id(f)
			self.canv.bookmarkPage(key)
			self.canv.addOutlineEntry(re.sub("<[^>]+>", "", text), key, level=lvl)
			self.notify("TOCEntry", (lvl, re.sub("<[^>]+>", "", text), self.page, key))

# ================================================================== CONTENT
# ---- cover
from reportlab.platypus import NextPageTemplate
story.append(Spacer(1, 30 * mm))
P("LPC1768 Complete Guide", "title"); SP(6)
P("mbed NXP LPC1768 + mbed Application Board<br/>Mbed Studio  |  Mbed OS 6  |  C++", "sub"); SP(14)
IMGF("pinout.png", 0.78)
SP(6)
P("RBB3023 Embedded &amp; IoT Systems  -  Test 2 preparation<br/>Chan Li Kai  |  November 2026", "sub")
SP(10)
TABLE([["What is inside", ""],
       ["Board, pins, active levels", "How to build in Mbed Studio and from the command line"],
       ["How to flash (copy .bin + reset), serial monitor", "Every Mbed OS 6 class with its functions"],
       ["11 example programs (all compiled), LCD views", "Interrupt rules, scheduled programming theory"],
       ["Worked Test-1-style answer + a practice question", "RTOS: threads, mutex, mail, flags, event queue"],
       ["18 example programs (6 of them RTOS)", "Errors, troubleshooting, test-day checklist"],
       ["16 copy-paste code modules (section 15)", "Code_Modules.txt for easy copying"]],
      [0.5, 0.5], head=True, zebra=True)
story.append(NextPageTemplate("main"))
story.append(PageBreak())
P("Contents", "h1")
toc = TableOfContents(); toc.levelStyles = [sty["toc1"], sty["toc2"]]; toc.dotsMinLevel = 0
story.append(toc)

# ---- 1 board
H1("1. The board at a glance")
P("The test uses the <b>mbed NXP LPC1768</b> module plugged into the <b>mbed Application Board</b>. "
  "The LPC1768 is the small blue module with the USB socket. The Application Board around it gives you the LCD, "
  "joystick, two potentiometers, RGB LED, speaker and two I2C sensors. You program it in C++ with <b>Mbed OS 6</b>.")
TABLE([["Item", "LPC1768 (mbed module)"],
       ["CPU", "ARM Cortex-M3, 96 MHz, ARMv7-M (Thumb-2), 3-stage pipeline, Harvard architecture"],
       ["Memory", "512 KB flash, 64 KB RAM"],
       ["ADC / DAC", "12-bit ADC (6 channels on p15-p20), 10-bit DAC on p18"],
       ["PWM", "6 outputs p21-p26 (+ LED1-LED4); all share ONE period"],
       ["Serial", "4 UARTs: p9/p10, p13/p14, p28/p27 and the USB link (USBTX/USBRX)"],
       ["Other", "2 SPI, 2 I2C, Ethernet, USB device, CAN"],
       ["Programming", "USB drive called MBED (copy a .bin file onto it, press reset)"],
       ["Tool", "Mbed Studio (or the command line build script)"]], [0.2, 0.8])
H2("1.1 Pin map of the Application Board")
IMGF("pinout.png", 0.95, "Figure 1 - Every mbed pin and what the Application Board connects to it. Red = LCD: never use p5, p6, p7, p8, p11 for anything else.")
TABLE([["Device", "Pin(s)", "Class to use", "Active level / range"],
       ["LED1 - LED4 (blue, on module)", "LED1, LED2, LED3, LED4", "DigitalOut / PwmOut", "1 = ON"],
       ["RGB LED", "R p23, G p24, B p25", "DigitalOut / PwmOut", "<b>0 = ON</b> (active low)"],
       ["Joystick", "down p12, left p13, centre p14, up p15, right p16", "DigitalIn / InterruptIn (PullDown)", "1 = pressed"],
       ["Potentiometer 1 / 2", "p19 / p20", "AnalogIn", "0 - 3.3 V, 12-bit"],
       ["Speaker", "p26", "PwmOut", "tone = PWM period"],
       ["LCD 128 x 32 (C12832)", "MOSI p5, RESET p6, SCK p7, A0 p8, nCS p11", "LCD_C12832 (lcd_c12832.h)", "pixel 1 = dark"],
       ["Temperature LM75B", "SDA p28, SCL p27", "I2C", "address 0x90"],
       ["Accelerometer MMA7660", "SDA p28, SCL p27", "I2C", "address 0x98"],
       ["XBee socket / UART3", "TX p9, RX p10", "BufferedSerial", "free for board-to-board"],
       ["Analog out (DAC)", "p18", "AnalogOut", "0 - 3.3 V"],
       ["USB serial to PC", "USBTX, USBRX", "printf / BufferedSerial", "115200 baud (mbed_app.json)"]],
      [0.22, 0.30, 0.26, 0.22])
BOX("Difference from the NUC140 (Test 1 board)", [
    "There is <b>no 7-segment display, no 3x3 keypad and no push button</b>. The joystick replaces the keypad and buttons, "
    "the 128x32 LCD replaces the 7-segment, and LED1-LED4 are the running LEDs. The LCD is only 32 pixels high (NUC140 was 64).",
    "LED1-LED4 are <b>active high</b> (1 = on) but the RGB LED is <b>active low</b> (0 = on)."], "info")

# ---- 2 software
H1("2. Software and files")
H2("2.1 Where everything is")
TABLE([["What", "Where"],
       ["Mbed Studio workspace", "C:\\Users\\TUF FA707RC-HX024W\\Mbed Programs"],
       ["Ready program for the test", "Mbed Programs\\Test2 (shortcut in D:\\...\\15 Test 2)"],
       ["Shared Mbed OS 6 library", "C:\\Users\\TUF FA707RC-HX024W\\mbed-os (each program's mbed-os folder is a link to it)"],
       ["Command line build", "Mbed Programs\\build_test2.cmd"],
       ["This guide's examples", "D:\\...\\15 Test 2\\Guide_Examples (.cpp) and Guide_Examples\\bin (.bin, ready to flash)"],
       ["Lecture PDFs", "D:\\...\\15 Test 2\\Notes (08a Timer/Ticker, 09 ADC end, EIOTS_10 UART end)"]], [0.3, 0.7])
H2("2.2 Files inside a program")
TABLE([["File", "What it does", "Change it?"],
       ["main.cpp", "Your program. The only file you normally edit.", "Yes"],
       ["lcd_c12832.h", "Our own LCD driver for Mbed OS 6 (text, pixel, box, fill_box, update).", "No"],
       ["font5x7.h", "5x7 font used by the LCD driver (from the NUC140 BSP).", "No"],
       ["mbed_app.json", "Settings: serial baud 115200 for printf.", "Only to change baud"],
       [".mbedignore", "Skips unused Mbed OS parts (cellular, LoRa, NFC, BLE, WiFi) so builds are faster.", "No (never ignore nanostack)"],
       ["mbed-os (link)", "Mbed OS 6.17 source. Shared by all programs.", "Never"],
       [".mbed, mbed-os.lib", "Target LPC1768 and the Mbed OS version.", "No"],
       ["BUILD\\LPC1768\\ARMC6", "Build output: <b>Test2.bin</b> is the file you flash.", "Generated"]],
      [0.2, 0.62, 0.18])
P("mbed_app.json in every program:")
CODE('{\n    "target_overrides": {\n        "*": {\n            "platform.stdio-baud-rate": 115200,\n            "platform.stdio-convert-newlines": false\n        }\n    }\n}')
H2("2.3 Making a new program (if you need one)")
OL(["Easiest: copy the whole <b>Test2</b> folder in Mbed Programs, rename the copy, then make a new <b>mbed-os</b> link inside it "
    "(PowerShell: " + c('New-Item -ItemType Junction -Path <copy>\\mbed-os -Target C:\\Users\\TUF FA707RC-HX024W\\mbed-os') + "). Delete the copied BUILD folder.",
    "Or in Mbed Studio: File &gt; New Program &gt; 'empty Mbed OS program', choose <b>link to an existing shared Mbed OS instance</b> "
    "and pick C:\\Users\\TUF FA707RC-HX024W\\mbed-os. Then copy lcd_c12832.h, font5x7.h, mbed_app.json and .mbedignore from Test2.",
    "On test day you don't need a new program: just replace Test2's main.cpp."])

# ---- 3 build
H1("3. How to build in Mbed Studio")
P("These screenshots are from Mbed Studio on this laptop with the <b>Test2</b> program. The numbers match the steps.")
IMGF("a_overview.png", 1.0, "Figure 2 - Mbed Studio main window: (1) Active program, (2) Target, (3) Build profile, (4) Build button, (5) main.cpp in the file tree, (6) code editor, (7) Output panel (build messages).")
H2("3.1 Step by step")
OL(["<b>Active program (1)</b>: click the box and pick <b>Test2</b>. The build button always builds the active program, "
    "not the file you are looking at.",
    "<b>Target (2)</b>: must say <b>mbed LPC1768</b>. If it doesn't, click it and type LPC1768 in the search box.",
    "<b>Build profile (3)</b>: Debug works. Develop is a bit smaller/faster; either is fine for the test.",
    "Open <b>main.cpp (5)</b> from the tree, edit, press <b>Ctrl+S</b> to save.",
    "Click the <b>hammer (4)</b> to build. Watch the <b>Output (7)</b> panel.",
    "Success = popup <i>'Build of Test2 completed: Program compiled'</i> and the last Output line "
    + c("Image: BUILD/LPC1768/ARMC6\\Test2.bin") + "."])
story.append(KeepTogether([Table([[Image(os.path.join(IMG, "b_program.png"), width=TW * 0.49, height=TW * 0.49 * 520 / 800),
                                    Image(os.path.join(IMG, "c_target.png"), width=TW * 0.49, height=TW * 0.49 * 520 / 800)]],
                                  colWidths=[TW * 0.5, TW * 0.5]),
                            Paragraph("Figure 3 - Left: choose the Active program (1) - pick Test2 (2). Right: the Target list (1); the chip icon (2) detects a connected board.", sty["cap"])]))
story.append(KeepTogether([Table([[Image(os.path.join(IMG, "d_profile.png"), width=TW * 0.49, height=TW * 0.49 * 520 / 800),
                                    Image(os.path.join(IMG, "e_buildmenu.png"), width=TW * 0.49, height=TW * 0.49 * 520 / 800)]],
                                  colWidths=[TW * 0.5, TW * 0.5]),
                            Paragraph("Figure 4 - Left: Build profile Debug / Develop / Release. Right: the arrow next to the hammer has <b>Clean build</b> (use it if a build acts strangely).", sty["cap"])]))
IMGF("f_building.png", 1.0, "Figure 5 - While building: Output shows 'Building project Test2 (LPC1768, ARMC6)', bottom-right shows the Build task spinning.")
IMGF("g_build_done2.png", 0.9, "Figure 6 - Build finished: (1) the 'Program compiled' popup, (2) memory use and the .bin path.")
BOX("How long does a build take?", [
    "The first build of a program compiles all of Mbed OS: about <b>3-5 minutes</b>. Test2 has already been built, so on test day "
    "only main.cpp is compiled again: <b>about 10-30 seconds</b>. Changing the Build profile starts a full build again, so pick one and keep it."], "info")
H2("3.2 Red marks in the editor that are NOT errors")
IMGF("h_problems.png", 1.0, "Figure 7 - The Problems tab may show 'Compiler generates FPU instructions for a device without an FPU' on #include \"mbed.h\".")
P("This message comes from the editor's code checker, not the real compiler. The build in Figure 6 compiled fine with it showing. "
  "<b>Only trust the Output panel of a build.</b> Red squiggles under " + c("ms") + " in " + c("200ms") + " or under Mbed classes are the same thing.")
H2("3.3 Build from the command line (backup)")
P("If Mbed Studio misbehaves, the same compiler can be run from a terminal (Claude Code can run it for you):")
CODE('cmd //c "C:\\Users\\TUF FA707RC-HX024W\\Mbed Programs\\build_test2.cmd"')
IMGF("term_cmdbuild.png", 0.85, "Figure 8 - Command line build: 'exit 0' and the Image line mean success. The full log is in Test2\\build.log.")
H2("3.4 Reading compile errors")
P("Errors look like " + c("main.cpp:10:15: error: ...") + " = file, <b>line 10, column 15</b>. Always fix the <b>first</b> error first; later ones are often caused by it. "
  "Below are three real errors from the three most common mistakes:")
IMGF("term_errors.png", 1.0, "Figure 9 - Missing semicolon (line 10), a global called 'count' (clashes with std::count), and wait() which does not exist in Mbed OS 6.")
TABLE([["Error message", "Cause", "Fix"],
       ["expected ';' after expression", "missing ; at the end of the line above the ^", "add the ;"],
       ["reference to 'count' is ambiguous", "global variable named count (std::count exists)", "rename: ticks, num, presses ..."],
       ["use of undeclared identifier 'wait'", "Mbed OS 6 removed wait() / wait_ms()", "ThisThread::sleep_for(500ms) or wait_us(500000)"],
       ["no member named 'read_ms'", "old mbed 2 Timer API", "timer.elapsed_time() >= 200ms"],
       ["unknown type name 'Serial'", "Mbed OS 6 has no Serial class", "printf(...) or BufferedSerial"],
       ["no matching member function for call to 'attach'", "attach(&f, 0.5) with a float, or wrong function type", "attach(&f, 500ms); function must be void f()"],
       ["invalid operands ... 'std::chrono'", "comparing elapsed_time() with a plain int", "use 200ms, not 200"],
       ["L6218E undefined symbol", "function declared but not written, or spelt differently", "write the function / fix the name"]],
      [0.33, 0.37, 0.30])

# ---- 4 flash
H1("4. How to flash (download) to the board")
IMGF("flash_flow.png", 1.0, "Figure 10 - The whole cycle. There is no 'download' button needed: the mbed works like a USB stick.")
H2("4.1 Steps")
OL(["Plug the mbed into the laptop with the USB cable. A drive appears (on this laptop usually <b>E:</b>). "
    "It may have no name; it contains the file <b>MBED.HTM</b>.",
    "Build (section 3). Open the build folder: <b>Mbed Programs\\Test2\\BUILD\\LPC1768\\ARMC6</b>.",
    "Copy <b>Test2.bin</b> to the MBED drive (drag and drop, or right-click &gt; Copy, then Paste on the drive). The status LED on the mbed flashes while copying.",
    "Press the <b>reset button</b> on the mbed module (the small button in the middle of the module). The new program starts.",
    "If the board has a COM port, opening a serial terminal and sending a <b>break</b> also resets it."])
IMGF("07_explorer_bin.png", 0.92, "Figure 11 - The build folder in File Explorer. Test2.bin (34 KB) is the file to copy. Ignore .elf/.map.")
BOX("Rules for the MBED drive", [
    "The mbed always runs the <b>newest .bin</b> on the drive. Old .bin files can stay, but deleting them avoids confusion.",
    "Copy only the .bin, never the whole folder.",
    "Mbed Studio's Run button (play) may hang at 'waiting for debug probe' on this laptop. Don't wait: copy the .bin by hand.",
    "If nothing changes after reset: check you built the <b>active</b> program, check the time stamp of the .bin, copy again, press reset again."], "warn")
H2("4.2 Flash from the command line (Claude can do this)")
CODE('# Git Bash: find the MBED drive (the one with MBED.HTM), copy the .bin,\n# then press reset on the board\nBIN="C:/Users/TUF FA707RC-HX024W/Mbed Programs/Test2/BUILD/LPC1768/ARMC6/Test2.bin"\nfor d in d e f g h; do\n    [ -f /$d/MBED.HTM ] && cp "$BIN" /$d/ && echo "copied to $d:"\ndone')

# ---- 5 serial
H1("5. Serial monitor (printf to the PC)")
P(c("printf(...)") + " sends text over the USB cable to the PC at <b>115200 baud</b> (set in mbed_app.json). You need a terminal to see it:")
UL(["<b>Mbed Studio</b>: when the board is connected, open the Serial Monitor (View menu / the board's serial tab) and set baud 115200.",
    "<b>Tera Term / PuTTY</b>: Serial, port = the mbed's COM port (Device Manager &gt; Ports: 'mbed Serial Port (COMx)', was COM9 on this laptop), 115200, 8 data bits, no parity, 1 stop bit.",
    "<b>Python</b> (pyserial is installed): " + c("py -m serial.tools.miniterm COM9 115200")])
IMGF("term_ex06.png", 0.8, "Figure 12 - Expected output of example 06 (pots) in a serial monitor.")
BOX("printf rules", [
    "End lines with " + c("\\r\\n") + " (mbed_app.json turns off automatic newline conversion).",
    "<b>%f does not work by default</b> (floats are left out to save space). Print integers: " + c('printf("%d.%02d V", mv/1000, mv%1000/10)') + ".",
    "<b>Never</b> call printf inside a Ticker / Timeout / InterruptIn function - the board crashes (section 7)."], "warn")

# ---- 6 program structure
H1("6. Program structure and Mbed OS 6 basics")
H2("6.1 The shape of every program")
CODE("""//
// header comment: what the program does + which pins (marked in tests)
//
#include "mbed.h"			// all Mbed classes
#include "lcd_c12832.h"			// only if you use the LCD

// 1. objects for the hardware (global)
DigitalOut led1(LED1);
DigitalIn  joy_up(p15, PullDown);
LCD_C12832 lcd;
Ticker     tk;

// 2. variables shared with interrupts must be volatile
volatile int ticks = 0;

// 3. interrupt functions: short, only change variables / DigitalOut
void on_tick() { ticks++; }

int main()
{
	// 4. set-up, runs once
	lcd.init();
	tk.attach(&on_tick, 500ms);

	// 5. forever loop: read inputs, update outputs, draw the LCD
	while (true) {
		led1 = joy_up;
		ThisThread::sleep_for(20ms);
	}
}""")
H2("6.2 Time values (std::chrono)")
P("Mbed OS 6 uses time values with units. Write the unit directly after the number:")
TABLE([["Write", "Means", "Used in"],
       ["500ms", "500 milliseconds", "attach, sleep_for, elapsed_time() comparison"],
       ["1s, 2s", "seconds", "attach(&f, 1s)"],
       ["100us", "microseconds", "attach(&f, 100us)"],
       ["std::chrono::milliseconds(x)", "a time from a variable x", "attach(&f, std::chrono::milliseconds(delay_ms))"],
       ["wait_us(1000)", "busy wait 1000 us (plain int)", "short delays"]], [0.33, 0.30, 0.37])
H2("6.3 Lecture code (mbed 2) to Mbed OS 6")
P("The lecture slides use the old mbed 2 API. It does not compile in Mbed Studio. Translate like this:")
TABLE([["Lecture (mbed 2)", "Mbed OS 6 (use this)"],
       ["wait(0.2);", "ThisThread::sleep_for(200ms);"],
       ["wait_ms(200);", "ThisThread::sleep_for(200ms);"],
       ["timer1.read_ms() >= 200", "timer1.elapsed_time() >= 200ms"],
       ["timer1.read()", "duration_cast&lt;milliseconds&gt;(timer1.elapsed_time()).count() / 1000.0"],
       ["flipper.attach(&amp;flip, 0.2);", "flipper.attach(&amp;flip, 200ms);"],
       ["flipper.attach_us(&amp;flip, 500);", "flipper.attach(&amp;flip, 500us);"],
       ["Serial pc(USBTX, USBRX); pc.printf(...)", "printf(...)   (baud in mbed_app.json)"],
       ["pc.baud(9600); pc.getc(); pc.putc(c);", "BufferedSerial pc(USBTX, USBRX, 9600); pc.read(&amp;c,1); pc.write(&amp;c,1);"],
       ["pc.readable()", "pc.readable()  (same)"],
       ["printf(\"%f\", x);", "printf(\"%d\", (int)(x * 1000));"],
       ["DigitalOut out(p5);", "DigitalOut out(LED1);  (p5 is the LCD)"],
       ["AnalogIn Ain(PA_0);  (Nucleo)", "AnalogIn pot1(p19);"]], [0.45, 0.55])

# ---- 7 API
H1("7. Every class you need (API reference)")
P("All of these come from " + c('#include "mbed.h"') + ". 'ISR safe' = allowed inside a Ticker / Timeout / InterruptIn function.")
H2("7.1 DigitalOut - LEDs and any output pin")
TABLE([["Function", "What it does", "ISR safe"],
       ["DigitalOut led(LED1);", "create output (starts at 0)", "-"],
       ["DigitalOut g(p24, 1);", "create with a start value (1 = RGB green off)", "-"],
       ["led = 1;  led.write(1);", "set the pin high / low", "yes"],
       ["led = !led;", "toggle", "yes"],
       ["int v = led;  led.read()", "read back what you wrote", "yes"]], [0.35, 0.5, 0.15])
H2("7.2 DigitalIn - joystick and any input pin")
TABLE([["Function", "What it does", "ISR safe"],
       ["DigitalIn up(p15, PullDown);", "create input with a pull-down (joystick = 1 when pressed)", "-"],
       ["if (up) ...  up.read()", "0 or 1", "yes"],
       ["up.mode(PullUp / PullDown / PullNone)", "change the pull resistor", "yes"]], [0.35, 0.5, 0.15])
P("<b>Edge detection</b> (one action per press, like " + c("last_key") + " on the NUC140):")
CODE("bool now = joy_centre;\nif (now && !last) { presses++; }	// only on the 0 -> 1 change\nlast = now;")
H2("7.3 InterruptIn - run a function the moment a pin changes")
TABLE([["Function", "What it does"],
       ["InterruptIn btn(p14);", "create (joystick centre)"],
       ["btn.mode(PullDown);", "pull resistor"],
       ["btn.rise(&amp;on_press);", "call on_press when the pin goes 0 -> 1 (pressed)"],
       ["btn.fall(&amp;on_release);", "call on_release when it goes 1 -> 0 (released)"],
       ["btn.rise(NULL);", "stop the interrupt"],
       ["btn.read()", "current level"]], [0.4, 0.6])
P("Joystick contacts bounce, so one press can call the function several times. Ignore calls within 200 ms using a Timer (example 04). "
  "On the LPC1768 only port 0 and port 2 pins can interrupt: p5-p18 and p21-p30 can, <b>p19 and p20 cannot</b>.")
H2("7.4 AnalogIn - potentiometers (ADC)")
TABLE([["Function", "Returns"],
       ["AnalogIn pot1(p19);", "create (POT1; POT2 = p20; also p15-p18 on other boards)"],
       ["pot1.read()   or   float f = pot1;", "0.0 - 1.0  (x 3.3 = volts)"],
       ["pot1.read_u16()", "0 - 65535; use >> 4 to get the real 12-bit 0 - 4095"],
       ["mV = raw * 3300 / 4095", "millivolts from the 12-bit value"],
       ["pct = raw * 100 / 4095", "percentage"]], [0.4, 0.6])
H2("7.5 AnalogOut - DAC on p18")
TABLE([["Function", "What it does"],
       ["AnalogOut dac(p18);", "create (only p18)"],
       ["dac = 0.5f;  dac.write(0.5f);", "output 0.5 x 3.3 V = 1.65 V"],
       ["dac.write_u16(32768);", "same using 0 - 65535"]], [0.4, 0.6])
H2("7.6 PwmOut - brightness, tones (p21-p26, LED1-LED4)")
TABLE([["Function", "What it does"],
       ["PwmOut red(p23);", "create"],
       ["red.period_ms(10);  red.period(0.01f);  red.period_us(100);", "PWM period (frequency = 1 / period)"],
       ["red = 0.25f;  red.write(0.25f);", "duty cycle 0.0 - 1.0 (25 % high)"],
       ["red.pulsewidth_ms(1);  red.pulsewidth_us(1500);", "set high time directly (servo: 1 - 2 ms in 20 ms)"],
       ["red.read()", "current duty"]], [0.55, 0.45])
BOX("PWM gotchas on the LPC1768", [
    "All PwmOut pins share <b>one period</b>. Setting the speaker to 440 Hz also changes the RGB period: set it back afterwards (example 07).",
    "RGB is active low: brightness b (0-1) is " + c("red = 1.0f - b;") + ".",
    "LED1 and p26 (speaker) use the same PWM channel: don't PWM both."], "warn")
H2("7.7 Timer - measure time (polling)")
TABLE([["Function", "What it does"],
       ["Timer t;  t.start();", "start counting"],
       ["t.stop();  t.reset();", "pause / set back to 0 (keeps running if started)"],
       ["t.elapsed_time()", "time so far (chrono microseconds); compare with 200ms, 1s ..."],
       ["duration_cast&lt;milliseconds&gt;(t.elapsed_time()).count()", "time as a plain number of ms"],
       ["if (t.elapsed_time() &gt;= 200ms) { task(); t.reset(); }", "the scheduled-programming pattern (lecture 08a)"]], [0.55, 0.45])
H2("7.8 Ticker - call a function again and again (interrupt)")
TABLE([["Function", "What it does"],
       ["Ticker tk;", "create"],
       ["tk.attach(&amp;f, 500ms);", "call f every 500 ms, starting 500 ms from now"],
       ["tk.detach();", "stop"],
       ["tk.detach(); tk.attach(&amp;f, std::chrono::milliseconds(ms));", "change the rate"]], [0.55, 0.45])
H2("7.9 Timeout - call a function once, later")
TABLE([["Function", "What it does"],
       ["Timeout to;  to.attach(&amp;f, 2s);", "call f once, 2 s from now"],
       ["to.detach();", "cancel"]], [0.55, 0.45])
H2("7.10 Waiting")
TABLE([["Function", "What it does", "Inside ISR?"],
       ["ThisThread::sleep_for(200ms);", "sleep, CPU free for other threads (best in main)", "NO"],
       ["wait_us(500);", "busy wait in microseconds (int)", "only very short"],
       ["while (t.elapsed_time() &lt; 100ms) {}", "busy wait with a Timer (worked on the old board)", "no"]], [0.42, 0.43, 0.15])
H2("7.11 BufferedSerial - UART (PC or another board)")
TABLE([["Function", "What it does"],
       ["BufferedSerial pc(USBTX, USBRX, 115200);", "USB serial to PC"],
       ["BufferedSerial link(p9, p10, 9600);", "UART3 to another board (TX p9, RX p10)"],
       ["pc.set_blocking(false);", "read() returns at once when no data"],
       ["pc.write(buf, len);", "send bytes"],
       ["n = pc.read(&amp;c, 1);", "receive (n = bytes read, -11 = none yet when non-blocking)"],
       ["pc.readable()", "true if data is waiting"],
       ["pc.set_baud(9600);  pc.set_format(8, BufferedSerial::None, 1);", "baud, data bits, parity, stop bits"]], [0.55, 0.45])
P("Use <b>either</b> printf <b>or</b> a BufferedSerial on USBTX/USBRX in one program. Two boards: TX to RX crossed, and <b>GND to GND</b>.")
H2("7.12 I2C - sensors on p28/p27")
TABLE([["Function", "What it does"],
       ["I2C i2c(p28, p27);", "SDA, SCL"],
       ["i2c.frequency(100000);", "100 kHz"],
       ["i2c.write(addr, data, len, repeated);", "returns 0 = OK (addr is 8-bit: 0x90 = LM75B)"],
       ["i2c.read(addr, buf, len);", "returns 0 = OK"]], [0.45, 0.55])
H2("7.13 LCD_C12832 - our LCD driver (lcd_c12832.h)")
TABLE([["Function", "What it does"],
       ["LCD_C12832 lcd;", "create (pins are built in)"],
       ["lcd.init();", "once at the start of main"],
       ["lcd.clear();", "clear the buffer (not the screen yet)"],
       ["lcd.text(x, y, \"Hi\", scale);", "5x7 text; scale 1 = 6x8 px per char (21 chars per line), 2 = 12x16, 3 = 18x24"],
       ["lcd.pixel(x, y, on);", "one pixel (on = false erases)"],
       ["lcd.box(x, y, w, h);", "rectangle outline"],
       ["lcd.fill_box(x, y, w, h, on);", "filled rectangle (on = false erases)"],
       ["lcd.update();", "send the buffer to the screen (about 0.3 ms)"]], [0.4, 0.6])
P("Screen = 128 x 32 pixels, (0,0) top-left, x to the right (0-127), y down (0-31). Text lines at y = 0, 11, 22 (or 0, 8, 16, 24). "
  "<b>Pattern:</b> clear, draw everything, update once per loop. Never call update() from a Ticker.")
H2("7.14 Classes you should NOT use in this course")
UL(["<b>BusOut / BusIn</b> - work in main, but crash inside Ticker callbacks (mutex). Use an array " + c("DigitalOut leds[4]") + " instead.",
    "<b>Serial</b>, <b>wait()</b>, <b>wait_ms()</b> - removed in Mbed OS 6.",
    "<b>Thread / Mutex / EventQueue</b> - these are the RTOS. They work well but have their own rules: see <b>section 11</b> before using them."])

# ---- 8 interrupts
H1("8. Interrupts and scheduled programming")
H2("8.1 The one rule that crashes the board")
BOX("Inside a Ticker / Timeout / InterruptIn function you may ONLY:", [
    "change " + c("volatile") + " variables, write DigitalOut / PwmOut, attach/detach Tickers.",
    "<b>Never</b>: printf, lcd.update(), sprintf to the LCD, BusOut, sleep_for, long loops, BufferedSerial write.",
    "If you break this, Mbed OS stops with a fatal error: the LEDs flash a pattern (or the program freezes after the first draw)."], "warn")
P("The safe pattern: the interrupt sets a flag or changes a number, the main loop does the slow work.")
CODE("""volatile bool tick_flag = false;
void on_tick() { tick_flag = true; }		// interrupt: fast

int main() {
	tk.attach(&on_tick, 1s);
	while (true) {
		if (tick_flag) {			// main: slow work
			tick_flag = false;
			printf("one second\\r\\n");
		}
	}
}""")
H2("8.2 Timer vs Ticker (lecture 08a)")
TABLE([["", "Timer (polling)", "Ticker (interrupt)"],
       ["How", "main keeps reading the time; runs the task when it is due", "hardware interrupt calls the function at a fixed interval"],
       ["CPU", "busy checking", "free for other code in main"],
       ["Accuracy", "depends on how long the main loop takes", "exact interval"],
       ["Risk", "a slow part of the loop delays every task", "function must be short; no printf/LCD inside"],
       ["Code", "if (t.elapsed_time() >= 200ms) {...; t.reset();}", "tk.attach(&amp;f, 200ms);"]], [0.14, 0.43, 0.43])
H2("8.3 Theory points for short answers")
UL(["Embedded systems need precise timing (a camera capturing frames at exact intervals) and fast response to events (a crash sensor firing an airbag).",
    "An <b>interrupt</b> halts the running code so a higher-priority routine runs. It can be <b>timed</b> (Ticker) or <b>external / event-driven</b> (InterruptIn, a mouse click).",
    "The simplest timer interrupt increments a counter in the background; main runs tasks when the counter reaches a value = <b>scheduled programming</b> (some tasks every 10 ms, others every 100 ms).",
    "Different rates are efficient: engine spark/injection every ~1 ms (up to 8000 rpm), fuel level every ~1000 ms.",
    "<b>Challenges</b>: every task must finish before its period ends or timing overruns and goes out of sync; the amount of code decides the CPU speed needed; "
    "tasks due at the same time need priorities (1 ms and 100 ms tasks meet every 100 ms); <b>no wait/delay (polling) inside scheduled tasks</b>.",
    "<b>Ticker advantage</b>: no need to read the time, so other code runs while the Ticker calls the function in the background.",
    "Ticker functions in the lecture: attach (seconds), attach_us (microseconds), member-function versions, detach."])

# ---- 9 examples
H1("9. Example programs (all compiled)")
P("Each example is a complete main.cpp in <b>15 Test 2\\Guide_Examples</b>, and its ready .bin is in <b>Guide_Examples\\bin</b>. "
  "To try one: copy its text over Test2's main.cpp, build, flash, or just copy the ready .bin to the MBED drive. "
  "All 17 (11 here + 6 RTOS ones in section 11) were compiled for the LPC1768 with Mbed OS 6.17 with <b>0 errors and 0 warnings in main.cpp</b>. "
  "The LCD pictures are drawn on the PC with the same drawing code as lcd_c12832.h, so they show exactly what the code draws.")
TABLE([["#", "File", "Shows", "Size"],
       ["01", "01_digitalout_blink.cpp", "DigitalOut array, running light, sleep_for", "25 KB"],
       ["02", "02_timer_two_rates.cpp", "Timer polling, two tasks at 200 ms / 1 s (lecture Ex 1-2)", "27 KB"],
       ["03", "03_ticker_speed.cpp", "3 Tickers, change Ticker rate with joystick (lecture Ex 3)", "28 KB"],
       ["04", "04_joystick_interrupt_timeout.cpp", "DigitalIn edges, InterruptIn + debounce, Timeout", "29 KB"],
       ["05", "05_lcd_drawing.cpp", "every LCD function, big number, growing bar", "32 KB"],
       ["06", "06_analogin_pots_serial.cpp", "AnalogIn pots, mV, %, bars, printf", "33 KB"],
       ["07", "07_pwm_rgb_speaker.cpp", "PwmOut RGB brightness, speaker tones, AnalogOut", "30 KB"],
       ["08", "08_serial_pc_commands.cpp", "BufferedSerial: commands from the PC", "31 KB"],
       ["09", "09_uart_two_boards.cpp", "UART3 p9/p10 between two boards", "36 KB"],
       ["10", "10_i2c_temp_accel.cpp", "I2C LM75B temperature + MMA7660 tilt", "34 KB"],
       ["11", "11_worked_example_test1_style.cpp", "Test 1 paper redone on this board (section 10)", "33 KB"],
       ["12", "12_rtos_threads.cpp", "RTOS: 3 threads + main at different rates (section 11)", "35 KB"],
       ["13", "13_rtos_mutex_lcd.cpp", "RTOS: Mutex - two threads share the LCD", "35 KB"],
       ["14", "14_rtos_mail_queue.cpp", "RTOS: Mail between threads, Queue from an ISR", "37 KB"],
       ["15", "15_rtos_eventflags_semaphore.cpp", "RTOS: EventFlags + Semaphore woken by ISRs", "37 KB"],
       ["16", "16_rtos_eventqueue.cpp", "RTOS: EventQueue - printf/LCD work posted from ISRs", "38 KB"],
       ["17", "17_rtos_worked_example.cpp", "RTOS: the Test 1 paper with one thread per task", "35 KB"],
       ["18", "18_modules_combined.cpp", "5 code modules joined: pot speed + LCD (section 15)", "33 KB"]],
      [0.06, 0.38, 0.46, 0.10])

def example(title, fname, lcd=None, lcdcap=None, term=None, termcap=None, notes=None, try_=None):
	H2(title)
	if notes: UL(notes)
	CODEFILE(fname)
	if lcd: IMGF(lcd, 0.62, lcdcap)
	if term: IMGF(term, 0.7, termcap)
	if try_: BOX("Try this", try_, "tip")

example("Example 01 - DigitalOut: running light", "01_digitalout_blink.cpp",
        notes=["An <b>array of DigitalOut</b> lets you use a loop and an index, like the 4 LEDs on the NUC140.",
               "On the board: one LED moves LED1 to LED4 and back, 200 ms per step."],
        try_=["Light two LEDs at a time. Change 200ms to 100ms. Make it go only one way (LED1 to LED4, then start again)."])
example("Example 02 - Timer: two tasks at two rates (lecture 08a Ex 1 + 2)", "02_timer_two_rates.cpp",
        notes=["Same structure as the lecture slide, translated to Mbed OS 6 and moved from p5/p6 to LED1/LED2.",
               "On the board: LED1 blinks fast (toggles every 0.2 s), LED2 slow (every 1 s)."],
        try_=["Add task3 on LED3 every 500 ms with a third Timer. What happens if you put ThisThread::sleep_for(300ms) inside task1? (Both rates go wrong - that is the 'no delays in scheduled programs' rule.)"])
example("Example 03 - Ticker: interrupts, and changing the speed (lecture 08a Ex 3)", "03_ticker_speed.cpp",
        term="term_ex03.png", termcap="Serial output while pressing joystick up twice and down three times.",
        notes=["Three Tickers run at the same time. main() is free; it only watches the joystick.",
               "To change a Ticker's speed: <b>detach()</b> then <b>attach()</b> with the new time.",
               "The joystick uses <b>edge detection</b>, so holding it doesn't keep changing the speed."],
        try_=["Make joystick left stop flipper2 (detach) and right start it again (attach)."])
example("Example 04 - Joystick: polling, InterruptIn and Timeout", "04_joystick_interrupt_timeout.cpp",
        notes=["Left/right are <b>polled</b> in main (DigitalIn). Centre uses an <b>interrupt</b> (InterruptIn.rise).",
               "The interrupt only changes variables and the RGB pin; printf happens in main via " + c("pressed_flag") + ".",
               "Timeout = one-shot: the green LED goes off 1 s after the last press."],
        try_=["Count presses of joystick up with another InterruptIn and show both counts with printf."])
example("Example 05 - LCD: every drawing function", "05_lcd_drawing.cpp", lcd="lcd_ex05.png",
        lcdcap="Figure - LCD of example 05 when the counter is 17: big number (scale 2), outline box, filled box with a hole, two pixels, bar on the right 17 px high.",
        notes=["Always: <b>clear() - draw - update()</b>. Drawing goes into memory; update() sends it to the screen.",
               "A bar that grows <b>up</b>: " + c("fill_box(x, 32 - h, w, h)") + ". A bar that grows to the right: " + c("fill_box(x, y, w, h)") + " with w growing."],
        try_=["Draw a box that moves left to right across the screen and bounces back (change x each loop, flip direction at 0 and 127-w)."])
example("Example 06 - AnalogIn: potentiometers to LCD and PC", "06_analogin_pots_serial.cpp", lcd="lcd_ex06.png",
        lcdcap="Figure - LCD of example 06 with POT1 at the middle (2048) and POT2 low (731).",
        term="term_ex06.png", termcap="Serial output every 0.5 s.",
        notes=["8 readings averaged = steady numbers. 12-bit = 0 to 4095.",
               "Volts printed as integers (" + c("mv / 1000") + " and " + c("mv % 1000 / 10") + ") because %f is off."],
        try_=["Light 1-4 LEDs depending on POT1 % (&lt;25, &lt;50, &lt;75, else) - that was Assignment 2."])
example("Example 07 - PwmOut: RGB brightness, speaker, AnalogOut", "07_pwm_rgb_speaker.cpp",
        notes=["Turn POT1: red gets brighter. POT2: blue. Joystick up/down: short beep at 440 / 880 Hz.",
               "A tone = PWM with period 1/f and duty 0.5."],
        try_=["Make POT2 control the beep pitch (200-2000 Hz). Play a 3-note tune on joystick centre."])
example("Example 08 - BufferedSerial: commands from the PC", "08_serial_pc_commands.cpp",
        term="term_ex08.png", termcap="Typing 1, 3, s and x in the serial monitor.",
        notes=["Non-blocking read: the loop never stops waiting for the PC, so the 2 s 'tick' keeps going.",
               "This program does not use printf because it owns USBTX/USBRX with BufferedSerial."],
        try_=["Add command 'r' that resets all LEDs, and 'p' that sends the POT1 value."])
example("Example 09 - UART between two boards", "09_uart_two_boards.cpp", lcd="lcd_ex09.png",
        lcdcap="Figure - LCD of example 09: this board sends P1=1532, it received P1=3870 from the other board.",
        notes=["Same program on both boards. Wires: p9 to the other p10, p10 to the other p9, GND to GND.",
               "Messages are text lines ending in \\n, so the receiver knows where each message ends.",
               "If RX stays '(nothing yet)': check TX/RX are crossed, GND is joined, both use 9600."],
        try_=["Send the joystick direction as a letter (U/D/L/R) and show it on the other board's LCD."])
example("Example 10 - I2C temperature and accelerometer (extra)", "10_i2c_temp_accel.cpp", lcd="lcd_ex10.png",
        lcdcap="Figure - LCD of example 10 lying flat (Z about 21 = 1 g).",
        term="term_ex10.png", termcap="Serial output; X and Z change when the board is tilted.",
        notes=["Not in the lectures, but the sensors are on the board - useful for a bonus or the project.",
               "Addresses in Mbed are 8-bit (7-bit address shifted left): LM75B 0x48 to 0x90, MMA7660 0x4C to 0x98."],
        try_=["Move a dot on the LCD with the tilt (x = 64 + X*2, y = 16 + Y)."])

# ---- 10 worked example
H1("10. Worked example: the Test 1 paper on the LPC1768")
P("Test 1 (NUC140) asked for: a Timer countdown on the 7-segment from 77 to 5 every 0.5 s, keypad keys to change the delay of a running-LED pattern, "
  "a Timer2 bar on the left of the LCD that grows over time, and a push button adding boxes beside the bar - <b>all working at the same time</b>. "
  "Here is the same paper done on the LPC1768 with this board's parts.")
TABLE([["Test 1 task", "NUC140 part", "LPC1768 version"],
       ["1 countdown 77 to 5, 0.5 s", "Timer1 + 7-segment", "Ticker t_count 500ms, big number on the LCD (scale 3)"],
       ["2 delay up / down", "keypad top-middle / bottom-middle", "joystick up = +50 ms, down = -50 ms; running light on LED1-LED4"],
       ["3 bar on the left grows", "Timer2 + LCD", "Ticker t_bar 300ms, bar x 0-9 grows 1 px up, restarts when full"],
       ["4 boxes beside the bar", "push button SW_INT", "joystick centre adds a 7x7 box (0 to 12, then back to 0)"]],
      [0.26, 0.28, 0.46])
H2("10.1 How it is designed")
UL(["<b>Tickers only change numbers</b> (" + c("number") + ", " + c("bar_h") + "), both " + c("volatile") + ".",
    "<b>The main loop</b> reads the joystick (with edge detection), moves the LEDs when the LED Timer is due, and redraws the whole LCD every 10 ms.",
    "Nothing in the loop waits for long, so all four tasks look simultaneous - this is what 'all tasks should work simultaneously' means in the marking."])
CODEFILE("11_worked_example_test1_style.cpp")
H2("10.2 What the LCD shows")
story.append(KeepTogether([
    Image(os.path.join(IMG, "lcd_ex11a.png"), width=TW * 0.6, height=TW * 0.6 * 220 / 796),
    Paragraph("Start: 77, empty bar, no boxes, delay 300 ms.", sty["cap"]),
    Image(os.path.join(IMG, "lcd_ex11b.png"), width=TW * 0.6, height=TW * 0.6 * 220 / 796),
    Paragraph("Later: 52, bar 17 px, centre pressed twice, delay changed to 450 ms (up pressed 3 times).", sty["cap"]),
    Image(os.path.join(IMG, "lcd_ex11c.png"), width=TW * 0.6, height=TW * 0.6 * 220 / 796),
    Paragraph("Later: 23, bar almost full, 8 boxes in two rows, delay 150 ms.", sty["cap"])]))
H2("10.3 Changing it for a different question")
TABLE([["If the question says ...", "Change"],
       ["count UP from 5 to 77", "start number = 5, Ticker does number++ and goes back to 5 after 77"],
       ["every 1 s / 0.25 s", "t_count.attach(&amp;count_down, 1s) / 250ms"],
       ["show on LEDs in binary", "leds[i] = (number >> i) &amp; 1;"],
       ["running LEDs go the other way", "led_pos = (led_pos + 3) % 4;"],
       ["bar grows to the right / along the bottom", "fill_box(0, 26, bar_w, 6) with bar_w 0 to 127"],
       ["boxes appear under the bar", "change the x/y formula in the boxes loop"],
       ["use the pot to set the speed", "delay_ms = 50 + pot1.read() * 950;"],
       ["print to the PC as well", "printf in main when number changes (never in the Ticker)"]], [0.4, 0.6])

# ---- 11 RTOS
H1("11. RTOS - Mbed OS threads")
P("Mbed OS 6 <b>is</b> a real-time operating system (based on Keil RTX). Every program you built already runs on it: "
  + c("main()") + " is a thread. RTOS means you can split the program into several <b>threads</b> - each one a function with its own "
  + c("while (true)") + " loop - and the RTOS <b>scheduler</b> switches between them. When a thread sleeps or waits, the others run. "
  "Nothing to install or turn on: just " + c('#include "mbed.h"') + ".")
IMGF("rtos_timeline.png", 1.0, "Figure - Example 12 on a timeline: four threads at four rates. Each runs for a moment, then sleeps; the CPU is idle the rest of the time.")
H2("11.1 Super loop, Ticker or threads?")
TABLE([["", "Super loop + Timer (polling)", "Ticker / InterruptIn", "RTOS threads"],
       ["Idea", "one while loop checks everything", "interrupt calls a short function", "one function per task, each with its own loop"],
       ["Timing", "depends on the whole loop", "exact", "good (sleep_for is accurate to ~1 ms)"],
       ["printf / LCD", "yes", "<b>no</b>", "yes (LCD needs a Mutex if 2+ threads draw)"],
       ["Waiting inside a task", "blocks everything", "not allowed", "fine - only that thread waits"],
       ["Sharing data", "simple variables", "volatile variables", "Mutex, Mail, Queue, EventFlags"],
       ["Best for", "small programs, tests", "exact periodic ticks, buttons", "many independent tasks, the project, bonus marks"]],
      [0.14, 0.27, 0.27, 0.32])
H2("11.2 Words you need (theory)")
TABLE([["Word", "Meaning"],
       ["Thread (task)", "an independent function with its own stack, run by the RTOS"],
       ["Scheduler", "the RTOS part that decides which ready thread runs next"],
       ["Priority", "osPriorityLow / Normal / AboveNormal / High / Realtime. The highest-priority READY thread always runs"],
       ["Pre-emption", "a higher-priority thread that becomes ready interrupts a lower one immediately"],
       ["Round robin", "threads of the same priority share the CPU in turns (time slices)"],
       ["Context switch", "saving one thread's registers/stack pointer and loading another's"],
       ["States", "<b>Running</b> (using the CPU), <b>Ready</b> (wants the CPU), <b>Waiting/Blocked</b> (sleeping, waiting for a mutex, flag, message), <b>Inactive</b> (finished / not started)"],
       ["Blocking call", "a call that puts the thread into Waiting: sleep_for, lock, acquire, wait_any, try_get_for"],
       ["Stack", "each thread's own memory for local variables and calls (default 4 KB in Mbed OS)"],
       ["Race condition", "two threads change the same data at the same time -> wrong values. Fix: Mutex"],
       ["Deadlock", "two threads each wait for a mutex the other holds -> both stop for ever"],
       ["Starvation", "a low-priority thread never runs because higher ones never sleep"],
       ["Idle thread", "runs when nothing else is ready; puts the CPU to sleep"]], [0.2, 0.8])
H2("11.3 RTOS API (Mbed OS 6)")
TABLE([["Class / function", "What it does", "ISR safe"],
       ["Thread t(osPriorityNormal, 2048);", "create a thread object (priority, stack bytes)", "-"],
       ["t.start(task);", "start running void task() in the new thread", "no"],
       ["t.join();   t.terminate();", "wait for it to finish / kill it", "no"],
       ["ThisThread::sleep_for(100ms);", "this thread waits; others run", "no"],
       ["ThisThread::get_id(), t.get_state()", "which thread, its state", "no"],
       ["<b>Mutex</b> m;  m.lock();  m.unlock();", "only one thread at a time between lock and unlock", "<b>no</b>"],
       ["m.trylock_for(10ms)", "lock if possible within 10 ms (true/false)", "no"],
       ["<b>Semaphore</b> s(0);  s.acquire();  s.release();", "count of tokens; acquire waits for a token, release adds one", "release: yes"],
       ["s.try_acquire_for(1s)", "wait max 1 s (true/false)", "no"],
       ["<b>EventFlags</b> ef;  ef.set(0x1);", "set flag bits (wake waiting threads)", "yes"],
       ["ef.wait_any(0x3);   ef.wait_all(0x3);", "wait until any / all bits are set; returns the bits (and clears them)", "no"],
       ["<b>Mail</b>&lt;T, N&gt; mb;  T *p = mb.try_alloc();", "get an empty message slot (NULL if full)", "yes"],
       ["mb.put(p);   p = mb.try_get_for(1s);   mb.free(p);", "send / receive (NULL on timeout) / give the slot back", "put: yes"],
       ["<b>Queue</b>&lt;T, N&gt; q;  q.try_put(ptr);", "send a pointer (or a small number cast to void*)", "yes"],
       ["q.try_get_for(1s, &amp;ptr)", "receive, true if something came", "0ms: yes"],
       ["<b>EventQueue</b> eq(32 * EVENTS_EVENT_SIZE);", "a list of functions to run later in one thread", "-"],
       ["eq.call(f);  eq.call_in(2s, f);  eq.call_every(500ms, f);", "run f now / once later / repeatedly", "yes"],
       ["t.start(callback(&amp;eq, &amp;EventQueue::dispatch_forever));", "a thread that runs the queued functions", "-"]],
      [0.44, 0.44, 0.12])
H2("11.4 Which tool for which job")
TABLE([["You want to ...", "Use"],
       ["run 2-5 independent jobs at different rates", "one Thread each, loop + sleep_for (example 12)"],
       ["let two threads use the LCD / the same variables", "Mutex around the shared part (example 13), or only ONE thread draws (example 17)"],
       ["send readings from one thread to another", "Mail with a struct (example 14)"],
       ["get data out of an interrupt", "Queue.try_put or Mail.put from the ISR (example 14)"],
       ["wake a thread when a button is pressed", "EventFlags.set in the ISR, wait_any in the thread (example 15)"],
       ["wake a thread every second from a Ticker / count events", "Semaphore.release in the ISR, acquire in the thread (example 15)"],
       ["do printf / LCD because of an interrupt", "EventQueue: ISR calls queue.call(f) (example 16)"],
       ["periodic job that may printf", "queue.call_every(500ms, f) instead of a Ticker (example 16)"]], [0.5, 0.5])
H2("11.5 RTOS rules (read before the examples)")
BOX("Rules", [
    "<b>Every thread loop must block</b> (sleep_for, wait_any, acquire, try_get_for ...). A thread that spins in a loop with no wait starves all lower-priority threads. "
    "Don't use the Timer busy-wait loop inside threads.",
    "<b>Stack size</b>: 1024 bytes for a tiny loop, <b>2048</b> if the thread uses printf / sprintf, 4096 for an EventQueue thread. Too small = 'stack overflow' fatal error. Big arrays: make them global.",
    "The LPC1768 has about 20 KB free for thread stacks: keep to roughly <b>6 threads of 2 KB</b>.",
    "<b>Interrupts still cannot printf, lock a Mutex or wait</b>. From an ISR you may only: set EventFlags, release a Semaphore, Queue.try_put, Mail.try_alloc/put, EventQueue.call.",
    "printf is safe from threads (Mbed locks the serial port for you); lines from different threads may come out in any order.",
    "The LCD driver is NOT thread safe: either one thread does all the drawing, or every LCD use is inside the same Mutex.",
    "Keep mutex locks short; never sleep while holding one; if you need two mutexes, always lock them in the same order (no deadlock).",
    "Mbed OS 6 gotcha: " + c("btn.rise(queue.event(f))") + " does not compile ('F must be TriviallyCopyable'). Use an ISR with " + c("queue.call(f)") + "."], "warn")

def rtos_example(title, fname, lcd=None, lcdcap=None, notes=None, try_=None):
	example(title, fname, lcd=lcd, lcdcap=lcdcap, notes=notes, try_=try_)

rtos_example("Example 12 - Threads: four tasks at four rates", "12_rtos_threads.cpp", lcd="lcd_ex12.png",
        lcdcap="Figure - LCD of example 12 after about 30 s: the LED threads ran 150 and 60 times.",
        notes=["Three extra threads + main. Each thread = function with its own loop and sleep_for.",
               "The counters are only written by one thread each and read by main, so no mutex is needed here."],
        try_=["Give thread_pot osPriorityHigh and remove its sleep_for. What happens to the LEDs and the LCD? (They stop: starvation.) Put the sleep back."])
rtos_example("Example 13 - Mutex: two threads share one LCD", "13_rtos_mutex_lcd.cpp", lcd="lcd_ex13.png",
        lcdcap="Figure - Top half drawn by thread_pot, bottom half by thread_joy (joystick held left).",
        notes=["Both threads draw into the same buffer and use the same SPI bus. The Mutex makes each draw-and-update happen as one piece.",
               "Each thread erases only its own half (fill_box with false), not the whole screen."],
        try_=["Comment out the lock/unlock lines and run it: you will see flicker and broken pieces. Put them back."])
rtos_example("Example 14 - Mail and Queue: passing data", "14_rtos_mail_queue.cpp", lcd="lcd_ex14.png",
        lcdcap="Figure - Message #42 from the producer thread, and the last joystick button (3 = centre) from the ISR queue.",
        notes=["<b>Mail</b> carries a whole struct (copied into the mailbox). The receiver must call free().",
               "<b>Queue</b> carries pointers; here small numbers (1, 2, 3) are cast to void* so the ISR doesn't need memory.",
               "main waits with try_get_for(1s): it sleeps until a message arrives (no polling)."],
        try_=["Add a third field 'pot_percent' to the struct and show it. Make the producer send only when a pot changes by more than 50."])
rtos_example("Example 15 - EventFlags and Semaphore: waking threads from ISRs", "15_rtos_eventflags_semaphore.cpp", lcd="lcd_ex15.png",
        lcdcap="Figure - value changed to 70 with joystick up (2 presses), 37 seconds counted by the semaphore thread.",
        notes=["The ISRs are one line each. All slow work (printf, LCD) is in the threads.",
               "EventFlags = several yes/no events in one 32-bit word; wait_any returns which ones happened.",
               "Semaphore = a counter of tokens: the Ticker adds one per second, the thread takes one each time round."],
        try_=["Add FLAG_LEFT / FLAG_RIGHT (bits 3 and 4) that move a small box left/right."])
rtos_example("Example 16 - EventQueue: interrupts that may printf", "16_rtos_eventqueue.cpp", lcd="lcd_ex16.png",
        lcdcap="Figure - number = 3 after three 'up' presses, centre pressed less than 2 s ago.",
        notes=["Everything in the queue runs in one thread, one function at a time, so the LCD needs no mutex.",
               "call_every replaces a Ticker when the periodic job needs printf / LCD. call_in replaces a Timeout."],
        try_=["Use call_every(100ms, ...) to read POT1 and move a bar; keep the buttons working."])
H2("Example 17 - The Test 1 paper with RTOS threads")
P("Same behaviour and LCD layout as example 11 (section 10), but each task is its own thread. Compare the two:")
TABLE([["", "Example 11 (Tickers + super loop)", "Example 17 (RTOS threads)"],
       ["Countdown", "Ticker ISR changes a volatile int", "t_count thread: sleep_for(500ms), then change"],
       ["Running LEDs", "Timer polled in the main loop", "t_leds thread sleeps for led_delay"],
       ["Joystick", "edge detection in the main loop", "t_input thread (higher priority) every 20 ms"],
       ["Bar", "Ticker ISR", "t_bar thread"],
       ["LCD", "main loop every 10 ms", "main thread every 50 ms (only thread that draws)"],
       ["Shared data", "volatile", "data_mutex; main copies all values at once"],
       ["Good for", "short, fewer rules", "each task easy to read and change alone; shows RTOS knowledge"]], [0.16, 0.42, 0.42])
CODEFILE("17_rtos_worked_example.cpp")
P("The LCD looks exactly like example 11 (section 10.2).")
H2("11.6 RTOS questions for short answers")
TABLE([["Question", "Answer"],
       ["What is an RTOS?", "An OS that runs several threads and guarantees time limits by priority-based pre-emptive scheduling."],
       ["Thread vs ISR?", "A thread can block, wait and use printf/mutex; an ISR must be short and can't block. ISRs always beat threads."],
       ["Why a Mutex?", "To stop a race condition: only one thread at a time uses shared data or a device (e.g. the LCD)."],
       ["Mutex vs Semaphore?", "Mutex = one owner, lock/unlock by the same thread, for protection. Semaphore = counter of tokens, any thread/ISR can release, for signalling or limiting."],
       ["What is a deadlock? How to avoid it?", "Threads waiting for each other's locks for ever. Lock in a fixed order, hold locks briefly, use trylock_for."],
       ["What is starvation?", "A low-priority thread never runs because higher ones never block."],
       ["Why must threads sleep or wait?", "So lower-priority and equal threads get the CPU, and the idle thread can save power."],
       ["How to get data from an ISR to a thread?", "Queue.try_put / Mail.put, EventFlags.set, Semaphore.release or EventQueue.call."],
       ["Thread states?", "Running, Ready, Waiting (Blocked), Inactive."]], [0.32, 0.68])

# ---- 12 practice
H1("12. Practice question - try it yourself")
P("Do this one without looking at a solution (there is none in this guide on purpose). Give yourself 60 minutes like a real test. "
  "Use sections 6-9 and the examples as your reference.")
TABLE([["Task", "Input", "Output"],
       ["1", "Ticker every 1 s", "LCD shows a stopwatch <b>MM:SS</b> (scale 2) that counts up from 00:00"],
       ["2", "Joystick centre", "pause / resume the stopwatch; each pause prints <b>Paused at MM:SS</b> on the serial monitor"],
       ["3", "POT1", "speed of a running light on LED1 - LED4, from 100 ms (POT1 max) to 1000 ms (POT1 min); show the speed in ms on the LCD"],
       ["4", "Joystick left / right", "move a small 6x6 box along the bottom row of the LCD; it must stop at the edges"],
       ["5", "Joystick up", "reset the stopwatch to 00:00 and the RGB LED flashes green for 0.5 s (Timeout)"]],
      [0.08, 0.22, 0.70])
P("<b>All tasks must work at the same time.</b>")
H2("Hints")
UL(["Keep seconds in a " + c("volatile int") + " changed by the Ticker; minutes = s / 60, seconds = s % 60, " + c('sprintf(t, "%02d:%02d", ...)') + ".",
    "Pause = a " + c("volatile bool running") + "; the Ticker only counts when it is true (or detach/attach).",
    "printf for task 2 goes in main, after edge detection of the centre press - not in an interrupt.",
    "Task 3 is the same as example 11's LED code with " + c("delay_ms") + " computed from the pot every loop.",
    "Task 5: " + c("green = 0;") + " then " + c("to.attach(&green_off, 500ms);") + " (example 04)."])
H2("Self-check before you call it done")
UL(["Builds with 0 errors; header comment lists what it does and the pins.",
    "Stopwatch keeps exact time while you move the box and turn the pot (Ticker, not a sleep in the loop).",
    "One centre press = exactly one pause/resume (edge detection or debounce).",
    "The box cannot leave the screen; LEDs never stop running.",
    "Serial shows the pause line once per pause; nothing printed from interrupts."])

# ---- 12 troubleshooting
H1("13. Troubleshooting")
TABLE([["Problem", "Likely cause", "Fix"],
       ["No MBED drive appears", "cable is charge-only / loose; board not powered", "other USB cable or port; check the mbed's blue power LED"],
       ["Copied .bin but nothing changed", "reset not pressed; wrong program built; old .bin", "press reset; check the .bin time stamp; check Active program"],
       ["LEDs flash in a strange pattern / board stops", "Mbed OS fatal error: printf / LCD / BusOut / mutex inside a Ticker or InterruptIn", "move that code to main, use a volatile flag"],
       ["LCD blank", "lcd.init() missing, update() missing, or p5-p8/p11 used for something else", "init once, update after drawing, free the LCD pins"],
       ["LCD shows old pieces of text", "no lcd.clear() before drawing", "clear, draw, update every loop"],
       ["Pot always 0 or fixed", "module not seated well in the app board", "re-seat the LPC1768 module"],
       ["Joystick press counts many times", "contact bounce / holding the key", "edge detection or 200 ms debounce"],
       ["Joystick never reads 1", "missing PullDown, wrong pin", "DigitalIn x(p14, PullDown); check the pin table"],
       ["RGB LED is the wrong way round", "active low", "0 = on: write 1.0f - brightness"],
       ["Serial monitor shows rubbish", "wrong baud", "115200 (or what you set in BufferedSerial)"],
       ["Serial shows nothing", "wrong COM port; printf of %f; program crashed", "Device Manager &gt; Ports; print integers"],
       ["sleep_for seems very slow", "old interface firmware on one board (woke once per second)", "use a Timer busy-wait loop instead"],
       ["Mbed Studio Run hangs 'waiting for debug probe'", "pyOCD problem on this laptop", "ignore Run; copy the .bin by hand"],
       ["Build very slow every time", "profile changed, or Clean build", "keep the same profile; only main.cpp should recompile"],
       ["Red error in editor but build OK", "editor checker (FPU message)", "trust the Output panel only"],
       ["RTOS: fatal error 'stack overflow' / board stops after a thread starts", "thread stack too small (printf, sprintf, big local arrays)", "stack 2048 or 4096; make big arrays global"],
       ["RTOS: one thread never runs", "a higher-priority thread loops without sleeping or waiting", "every thread loop must sleep_for / wait / acquire"],
       ["RTOS: program freezes after a while", "deadlock (two mutexes locked in a different order) or a mutex never unlocked", "lock one mutex at a time; unlock on every path"],
       ["RTOS: LCD flickers / garbage", "two threads draw on the LCD without a mutex", "one LCD thread, or lock a Mutex around every LCD use"],
       ["RTOS: 'F must be TriviallyCopyable' compile error", "btn.rise(queue.event(f))", "ISR that calls queue.call(f) (example 16)"]],
      [0.28, 0.38, 0.34])

# ---- 13 checklist
H1("14. Test-day checklist")
OL(["Plug in the board. Check the MBED drive (MBED.HTM) appears.",
    "Flash the board check: copy <b>15 Test 2\\Test2_board_check.bin</b> to the drive, press reset. LCD shows 'Test 2 ready', pot values, joystick letters; LED1 blinks. Now you know the board, LCD and flashing all work.",
    "Open Mbed Studio, Active program = <b>Test2</b>, Target = <b>mbed LPC1768</b>.",
    "Read the whole question. List each task: input, output, timing. Decide: Ticker (exact timing) or Timer (polling) for each.",
    "If using Claude Code / Codex, give it the full question plus: <i>'LPC1768 + mbed Application Board, Mbed OS 6, program C:\\Users\\TUF FA707RC-HX024W\\Mbed Programs\\Test2, "
    "replace main.cpp, use lcd_c12832.h, no printf/LCD in Ticker callbacks, build with build_test2.cmd'</i>.",
    "Build. Fix the first error first. Flash. Press reset.",
    "Test every task <b>on the board</b>, and test them <b>together</b> (the marking checks they run simultaneously).",
    "Keep the header comment (what it does + pins).",
    "After the test: copy main.cpp and Test2.bin into 15 Test 2."])
H2("Things that cost marks last time")
UL(["Key / direction mapping: check on the real board which joystick direction is which before trusting the picture.",
    "Direction words in the paper (clockwise / counter-clockwise, increasing / decreasing): read twice, ask if unsure.",
    "Tasks that work alone but freeze when combined: usually a sleep or a long loop blocking the others."])
H2("Quick theory self-test")
TABLE([["Question", "Answer"],
       ["Timer vs Ticker?", "Timer = polling, main reads the time. Ticker = interrupt calls the function at a fixed rate; main is free."],
       ["Why no wait() in a scheduled program?", "A blocking delay stops every other task, so their timing overruns and goes out of sync."],
       ["What happens if a 1 ms task takes 2 ms?", "Timing overruns, the system goes out of sync / misses deadlines."],
       ["POT raw 2048 (12-bit), voltage?", "2048 x 3.3 / 4095 = 1.65 V"],
       ["read_u16() = 49152, 12-bit value?", "49152 >> 4 = 3072 (75 %)"],
       ["Speaker 1 kHz: period?", "1 / 1000 = 1 ms, duty 0.5"],
       ["RGB red half brightness?", "red = 0.5f (active low: 1 - 0.5)"],
       ["Two mbeds over UART: wiring?", "TX to RX, RX to TX, GND to GND, same baud"],
       ["How many UARTs on the LPC1768?", "4: three on pins, one to the USB link (USBTX/USBRX)"],
       ["Which mbed pins are analog in?", "p15 - p20 (6 channels, 12-bit)"]], [0.4, 0.6])

# ---- 15 code modules
import modules as MOD
H1("15. Code modules (copy and paste)")
P("Ready pieces of code. Each one is a few helper functions, like " + c("set_rgb()") + " and " + c("set_led()") +
  ". Paste the ones you need, then call them. All 16 were compiled one by one <b>and all together in one program</b> "
  "(0 errors, 0 warnings), so you can mix them.")
BOX("How to paste a module", [
    "Every module has up to three parts. Put each part in its place:",
    "<b>1. ABOVE main()</b> - objects, variables, functions.",
    "<b>2. SETUP</b> - inside main(), before " + c("while (true)") + " (runs once).",
    "<b>3. LOOP</b> - inside " + c("while (true)") + " (the LOOP part is only an example of how to call it: change it).",
    "If a module says <b>Needs</b>, paste that module too. For easy copying use <b>Code_Modules.txt</b> (keeps the tabs) instead of the PDF."], "info")
CODE("""#include "mbed.h"
#include "lcd_c12832.h"	// only if you use the LCD (M04, M05)

// 1. ABOVE main(): paste the module tops here

int main()
{
	// 2. SETUP: paste the module setup lines here

	while (true)
	{
		// 3. LOOP: your code, calling the module functions

		ThisThread::sleep_for(20ms);
	}
}""")
TABLE([["Module", "What it gives you", "Needs"]] +
      [[m["id"], m["title"], ", ".join(m["needs"]) or "-"] for m in MOD.MODULES], [0.1, 0.72, 0.18])
for m in MOD.MODULES:
	H2("%s - %s" % (m["id"], m["title"]))
	P(m["what"] + ("  <b>Needs:</b> " + ", ".join(m["needs"]) + "." if m["needs"] else ""))
	H3("1. Paste ABOVE main()")
	CODE(m["top"])
	if m["setup"].strip():
		H3("2. Paste in SETUP (before while)")
		CODE(m["setup"])
	if m["loop"].strip():
		H3("3. Use it in the LOOP (example)")
		CODE(m["loop"])
	BOX("Adjust", m["adjust"], "tip")
H2("Putting modules together (example 18)")
P("POT1 sets the speed of the running LEDs, and the LCD shows the speed and a bar. It is just M01 + M04 + M05 + M07 + M08 + M09 "
  "pasted in their places, plus 10 lines in the loop. Compiled with 0 errors.")
CODEFILE("18_modules_combined.cpp")
IMGF("lcd_ex18.png", 0.62, "Figure - LCD of example 18 with POT1 at about 60 %: speed 450 ms, LED3 lit.")

doc = Doc(OUT, title="LPC1768 Complete Guide", author="Chan Li Kai", subject="RBB3023 Test 2 - mbed LPC1768 + Application Board")
doc.multiBuild(story)
print("written", OUT)
