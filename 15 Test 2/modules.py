# modules.py : copy-paste code modules for the LPC1768 guide
#   each module: id, title, what, needs (other module ids), adjust (tips),
#   top  = paste ABOVE main()  (objects, variables, functions)
#   setup = paste inside main() BEFORE while (true)
#   loop  = paste inside while (true)
#   build_modules.py puts every module into a test program and compiles it.

MODULES = [
dict(id="M01", title="LED1 - LED4 helpers", needs=[],
what="Turn the 4 blue LEDs on/off, one at a time, as a number in binary, or as a level.",
adjust=["LED1 ~ LED4 are 1 = on.", "set_led(1,0,0,1) = LED1 and LED4 on."],
top="""
// LED1 ~ LED4 on the mbed module (1 = on)
DigitalOut led[4] = { DigitalOut(LED1), DigitalOut(LED2), DigitalOut(LED3), DigitalOut(LED4) };

// each LED on (1) or off (0)
void set_led(int a, int b, int c, int d)
{
	led[0] = a;
	led[1] = b;
	led[2] = c;
	led[3] = d;
}

// only LED number n on (0 = LED1 ... 3 = LED4), the rest off
void led_one(int n)
{
	for (int i = 0; i < 4; i++)
		led[i] = (i == n);
}

// first n LEDs on (0 ~ 4), like a level meter
void led_level(int n)
{
	for (int i = 0; i < 4; i++)
		led[i] = (i < n);
}

// number 0 ~ 15 in binary (LED1 = bit 0)
void led_binary(int value)
{
	for (int i = 0; i < 4; i++)
		led[i] = (value >> i) & 1;
}
""", setup="", loop="""
set_led(1, 0, 0, 1);		// LED1 + LED4 on
led_one(2);			// only LED3
led_level(3);			// LED1 ~ LED3
led_binary(5);			// 0101 -> LED1 + LED3
"""),

dict(id="M02", title="RGB LED helper", needs=[],
what="Set the RGB LED colour with 1 = on (the function handles active low).",
adjust=["Colours: (1,0,0) red, (0,1,0) green, (0,0,1) blue, (1,1,0) yellow, (1,0,1) purple, (0,1,1) cyan, (1,1,1) white, (0,0,0) off."],
top="""
// RGB LED on the app board: R p23, G p24, B p25 (0 = on), start off
DigitalOut rgb[3] = { DigitalOut(p23, 1), DigitalOut(p24, 1), DigitalOut(p25, 1) };

// 1 = colour on, 0 = off
void set_rgb(int r, int g, int b)
{
	rgb[0] = !r;
	rgb[1] = !g;
	rgb[2] = !b;
}
""", setup="", loop="""
set_rgb(1, 1, 0);		// yellow
"""),

dict(id="M03", title="Joystick: held and pressed-once", needs=[],
what="Read all 5 directions. 'held' = true while you hold it. 'pressed' = true only once per press (edge detection).",
adjust=["Use joy_up etc. for 'while held'. Use up_pressed etc. for 'each press does one thing'.",
        "Call read_joystick() once at the top of every loop."],
top="""
// joystick (1 = pressed)
DigitalIn joy_up(p15, PullDown), joy_down(p12, PullDown), joy_left(p13, PullDown);
DigitalIn joy_right(p16, PullDown), joy_centre(p14, PullDown);

// true for ONE loop when the button goes from released to pressed
bool up_pressed, down_pressed, left_pressed, right_pressed, centre_pressed;
bool last_up, last_down, last_left, last_right, last_centre;

void read_joystick()
{
	bool up = joy_up, down = joy_down, left = joy_left, right = joy_right, centre = joy_centre;

	up_pressed     = up && !last_up;
	down_pressed   = down && !last_down;
	left_pressed   = left && !last_left;
	right_pressed  = right && !last_right;
	centre_pressed = centre && !last_centre;

	last_up = up;  last_down = down;  last_left = left;  last_right = right;  last_centre = centre;
}
""", setup="", loop="""
read_joystick();
if (joy_up) {			// while UP is held
}
if (centre_pressed) {		// once per CENTRE press
}
"""),

dict(id="M04", title="LCD: text and numbers", needs=[],
what="Start the LCD and write 3 lines of text, including numbers.",
adjust=["21 characters per line. Lines at y = 0, 11, 22.",
        "Always: clear, draw everything, update once.", "Numbers: sprintf(lcd_text, \"...%d...\", value)."],
top="""
#include "lcd_c12832.h"		// put this line at the very top, under #include "mbed.h"

LCD_C12832 lcd;			// 128 x 32 pixels, (0,0) = top left
char lcd_text[24];		// for sprintf
""", setup="""
lcd.init();			// once
""", loop="""
int value = 123;		// (your number)
lcd.clear();
sprintf(lcd_text, "Value %d", value);
lcd.text(0, 0, lcd_text);	// line 1
lcd.text(0, 11, "line 2");	// line 2
lcd.text(0, 22, "line 3");	// line 3
lcd.update();
"""),

dict(id="M05", title="LCD: big number, bars, boxes", needs=["M04"],
what="Big number (scale 2 or 3), a bar that grows up, a bar that grows right, and a row of boxes.",
adjust=["Big number: scale 2 = 12 x 16 px per digit, scale 3 = 18 x 24 px.",
        "lcd_vbar: h = 0 ~ 32. lcd_hbar: value / max sets the length.",
        "lcd_boxes: per_row boxes in a row, then a new row."],
top="""
// big number at (x, y), scale 2 or 3
void lcd_big_number(int x, int y, int value, int scale)
{
	char t[12];
	sprintf(t, "%d", value);
	lcd.text(x, y, t, scale);
}

// bar at x, w pixels wide, grows UP, h = 0 ~ 32
void lcd_vbar(int x, int w, int h)
{
	lcd.box(x, 0, w, 32);			// frame
	lcd.fill_box(x + 1, 32 - h, w - 2, h);	// fill from the bottom
}

// bar at row y, 6 px high, full width, grows RIGHT, value 0 ~ max
void lcd_hbar(int y, int value, int max)
{
	lcd.box(0, y, 128, 6);
	lcd.fill_box(1, y + 1, value * 126 / max, 4);
}

// 'count' boxes of 7 x 7 px starting at (x, y), 'per_row' in a row
void lcd_boxes(int x, int y, int count, int per_row)
{
	for (int i = 0; i < count; i++)
		lcd.box(x + (i % per_row) * 10, y + (i / per_row) * 12, 7, 7);
}
""", setup="", loop="""
lcd.clear();
lcd_vbar(0, 10, 20);		// left bar, 20 px high
lcd_boxes(14, 4, 3, 6);		// 3 boxes beside it
lcd_big_number(88, 0, 77, 3);	// big 77 on the right
lcd.update();
"""),

dict(id="M06", title="Ticker: count every period", needs=[],
what="A Ticker changes a number in the background (count down 77 to 5 and repeat). Includes changing the speed.",
adjust=["Change 500ms, the start/end numbers, or count up instead (counter++).",
        "Only change variables in tick_count(): no printf / LCD there.",
        "set_tick_speed(ms) changes the period (detach + attach)."],
top="""
Ticker tick;
volatile int counter = 77;		// read it in main, e.g. show on the LCD

// runs every period in an interrupt: only change variables here
void tick_count()
{
	if (counter > 5)
		counter--;
	else
		counter = 77;
}

// change the Ticker period
void set_tick_speed(int ms)
{
	tick.detach();
	tick.attach(&tick_count, std::chrono::milliseconds(ms));
}
""", setup="""
tick.attach(&tick_count, 500ms);	// every 0.5 s
""", loop="""
int now = counter;			// use 'now' for the LCD / LEDs
"""),

dict(id="M07", title="Timer: do a task every N ms (changeable speed)", needs=[],
what="Polling timer for a task at a speed you can change while running, e.g. running LEDs whose delay the joystick changes.",
adjust=["task_ms is the speed: change it anywhere (joystick, pot ...).", "Put your task where it says."],
top="""
Timer task_timer;
int task_ms = 300;			// task period in ms
""", setup="""
task_timer.start();
""", loop="""
if (task_timer.elapsed_time() >= std::chrono::milliseconds(task_ms))
{
	task_timer.reset();
	// ---- your task here (e.g. next running LED) ----
}
"""),

dict(id="M08", title="Running LEDs (forward / backward / bounce)", needs=["M01"],
what="One step of a running light each time you call it. Call it from a Timer (M07) or a Ticker flag.",
adjust=["run_forward: LED1 -> LED4 -> LED1 ...", "run_backward: LED4 -> LED1 ...", "run_bounce: LED1 -> LED4 -> LED1 back and forth."],
top="""
int run_pos = 0, run_dir = 1;

void run_forward()  { run_pos = (run_pos + 1) % 4;  led_one(run_pos); }
void run_backward() { run_pos = (run_pos + 3) % 4;  led_one(run_pos); }
void run_bounce()
{
	run_pos += run_dir;
	if (run_pos >= 3 || run_pos <= 0)
		run_dir = -run_dir;
	led_one(run_pos);
}
""", setup="", loop="""
run_forward();
ThisThread::sleep_for(200ms);
"""),

dict(id="M09", title="Potentiometers (ADC)", needs=[],
what="Read POT1 / POT2 as 0 ~ 4095, and convert to %, millivolts, or any range (e.g. a speed).",
adjust=["map_pot(raw, 100, 1000) = 100 when the pot is at 0, 1000 at the top.",
        "Want it the other way round? map_pot(raw, 1000, 100)."],
top="""
AnalogIn pot1(p19), pot2(p20);

// average of 8 readings, 0 ~ 4095
int read_pot(AnalogIn &pot)
{
	int sum = 0;
	for (int i = 0; i < 8; i++)
		sum += pot.read_u16() >> 4;
	return sum / 8;
}

int pot_percent(int raw) { return raw * 100 / 4095; }		// 0 ~ 100
int pot_mv(int raw)      { return raw * 3300 / 4095; }		// 0 ~ 3300 mV
int map_pot(int raw, int lo, int hi) { return lo + (hi - lo) * raw / 4095; }	// lo ~ hi
""", setup="", loop="""
int p1 = read_pot(pot1);
int speed_ms = map_pot(p1, 100, 1000);
"""),

dict(id="M10", title="Serial: print to the PC every 0.5 s", needs=[],
what="printf to the serial monitor (115200 from mbed_app.json) at a fixed rate, with volts printed as integers.",
adjust=["End lines with \\r\\n. No %f: print mv / 1000 and mv % 1000.",
        "Never printf inside a Ticker / InterruptIn function."],
top="""
Timer print_timer;
""", setup="""
print_timer.start();
printf("\\r\\nProgram start\\r\\n");
""", loop="""
if (print_timer.elapsed_time() >= 500ms)
{
	print_timer.reset();
	int mv = 1650;					// (your value)
	printf("value = %d.%02d V\\r\\n", mv / 1000, mv % 1000 / 10);
}
"""),

dict(id="M11", title="Serial: read keys typed on the PC", needs=[],
what="BufferedSerial on USB: read one character without waiting, and send text back.",
adjust=["Don't use printf in the same program (this owns USBTX/USBRX). Use pc_print instead.",
        "pc_getc() returns -1 when nothing was typed."],
top="""
BufferedSerial pc(USBTX, USBRX, 115200);

void pc_print(const char *s) { pc.write(s, strlen(s)); }

// one character typed on the PC, or -1 if none (does not wait)
int pc_getc()
{
	char c;
	if (pc.readable() && pc.read(&c, 1) == 1)
		return c;
	return -1;
}
""", setup="""
pc.set_blocking(false);
pc_print("Type a key\\r\\n");
""", loop="""
int key = pc_getc();
if (key == '1')
	pc_print("you typed 1\\r\\n");
"""),

dict(id="M12", title="PWM: RGB brightness and speaker beep", needs=[],
what="Dim the RGB LED (0.0 ~ 1.0) and play a tone on the speaker. Use this INSTEAD of M02 (same pins).",
adjust=["brightness 0.0 = off, 1.0 = full.", "beep(440, 200) = 440 Hz for 200 ms (blocks for that time).",
        "All PWM pins share one period: beep() sets it back to 10 ms for the RGB."],
top="""
PwmOut pwm_r(p23), pwm_g(p24), pwm_b(p25);	// RGB (active low)
PwmOut speaker(p26);

// each colour 0.0 ~ 1.0
void set_rgb_pwm(float r, float g, float b)
{
	pwm_r = 1.0f - r;
	pwm_g = 1.0f - g;
	pwm_b = 1.0f - b;
}

void beep(int freq, int ms)
{
	speaker.period(1.0f / freq);
	speaker = 0.5f;
	ThisThread::sleep_for(std::chrono::milliseconds(ms));
	speaker = 0.0f;
	pwm_r.period_ms(10);			// shared period back for the RGB
}
""", setup="""
pwm_r.period_ms(10);
set_rgb_pwm(0, 0, 0);			// off
""", loop="""
set_rgb_pwm(0.5f, 0, 0);		// half red
"""),

dict(id="M13", title="Button interrupt + debounce + Timeout", needs=[],
what="Count joystick-centre presses by interrupt (debounced), and switch an LED off 1 s later with a Timeout.",
adjust=["Read press_flag in main (and set it back to false) to react to a press with printf / LCD.",
        "Change p14 to another joystick pin for another button."],
top="""
InterruptIn btn(p14);			// joystick centre
Timer       debounce;
Timeout     flash_timeout;
DigitalOut  flash_led(LED4);

volatile int  presses = 0;
volatile bool press_flag = false;

void flash_off() { flash_led = 0; }

void on_press()				// interrupt: keep it short
{
	if (debounce.elapsed_time() < 200ms)	// ignore switch bounce
		return;
	debounce.reset();
	presses++;
	press_flag = true;
	flash_led = 1;
	flash_timeout.attach(&flash_off, 1s);
}
""", setup="""
debounce.start();
btn.mode(PullDown);
btn.rise(&on_press);
""", loop="""
if (press_flag)
{
	press_flag = false;
	printf("pressed %d times\\r\\n", presses);
}
"""),

dict(id="M14", title="UART between two boards (text lines)", needs=[],
what="Send a text line to the other board and receive its lines. Wire p9 -> other p10, p10 <- other p9, GND - GND.",
adjust=["Both boards: same baud (9600).", "End every message with \\n.",
        "link_receive_line() returns true when a full line is in rx_line."],
top="""
BufferedSerial link(p9, p10, 9600);	// TX, RX
char rx_line[32];
int  rx_len = 0;

void link_send(const char *s) { link.write(s, strlen(s)); }

// call every loop; true when a whole line arrived (now in rx_line)
bool link_receive_line()
{
	char c;
	while (link.read(&c, 1) == 1)
	{
		if (c == '\\n') {
			rx_line[rx_len] = 0;
			rx_len = 0;
			return true;
		}
		if (rx_len < 30)
			rx_line[rx_len++] = c;
	}
	return false;
}
""", setup="""
link.set_blocking(false);
""", loop="""
link_send("HELLO\\n");
if (link_receive_line()) {
	// rx_line holds the text, e.g. compare: if (strcmp(rx_line, "HELLO") == 0) ...
}
"""),

dict(id="M15", title="RTOS: a thread and a mutex", needs=[],
what="Run a job in its own thread at its own rate. Protect shared numbers with a Mutex.",
adjust=["Copy the thread block for more threads (new names).", "Stack 2048 if the thread uses printf / sprintf.",
        "Every thread loop needs a sleep_for."],
top="""
Thread worker(osPriorityNormal, 2048);
Mutex  data_mutex;
int    shared_value = 0;			// used by worker and main

void worker_task()
{
	while (true)
	{
		data_mutex.lock();
		shared_value++;			// ---- your job here ----
		data_mutex.unlock();
		ThisThread::sleep_for(100ms);
	}
}
""", setup="""
worker.start(worker_task);
""", loop="""
data_mutex.lock();
int v = shared_value;			// take a copy, then unlock
data_mutex.unlock();
"""),

dict(id="M16", title="RTOS: EventQueue (interrupt -> printf / LCD)", needs=[],
what="Let an interrupt or a timer run code that is allowed to printf and draw, by posting it to an EventQueue thread.",
adjust=["Put slow work in the functions run by the queue.", "call_every replaces a Ticker; call_in replaces a Timeout."],
top="""
EventQueue queue(32 * EVENTS_EVENT_SIZE);
Thread     queue_thread(osPriorityNormal, 4096);
InterruptIn q_btn(p15);			// joystick up

void every_second() { printf("1 s\\r\\n"); }		// runs in queue_thread
void on_up_job()    { printf("up pressed\\r\\n"); }
void on_up_isr()    { queue.call(on_up_job); }		// interrupt: just post
""", setup="""
q_btn.mode(PullDown);
q_btn.rise(&on_up_isr);
queue.call_every(1s, every_second);
queue_thread.start(callback(&queue, &EventQueue::dispatch_forever));
""", loop=""),
]

TEMPLATE = """#include "mbed.h"
{includes}
{top}

int main()
{{
{setup}
	while (true)
	{{
{loop}
		ThisThread::sleep_for(20ms);
	}}
}}
"""

def indent(code, n):
	return "\n".join(("\t" * n + ln) if ln.strip() else "" for ln in code.strip("\n").split("\n"))

def assemble(mod):
	"""full test program for one module (with the modules it needs)"""
	by_id = {m["id"]: m for m in MODULES}
	parts = [by_id[d] for d in mod["needs"]] + [mod]
	inc = '#include "lcd_c12832.h"' if any(p["id"] in ("M04",) for p in parts) else ""
	top = "\n".join(p["top"].replace('#include "lcd_c12832.h"\t\t// put this line at the very top, under #include "mbed.h"\n', "") for p in parts)
	setup = "\n".join(indent(p["setup"], 1) for p in parts if p["setup"].strip())
	loop = "\n".join(indent(p["loop"], 2) for p in parts if p["loop"].strip())
	return TEMPLATE.format(includes=inc, top=top, setup=setup, loop=loop)
