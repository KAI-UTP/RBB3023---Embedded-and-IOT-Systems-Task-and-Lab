# Test 2 Study Guide: mbed LPC1768 + Application Board (Mbed Studio, Mbed OS 6)

Course RBB3023 Embedded & IoT Systems, lecturer Patrick Sebastian. Same rules as Test 1: using Claude Code or Codex to write the code is allowed.
This time the board is the **mbed NXP LPC1768 on the mbed Application Board** (not the Nuvoton NUC140, not the Nucleo).

| Item | Where |
|---|---|
| **Full PDF guide** | `LPC1768_Complete_Guide.pdf` (51 pages: board, build/flash with screenshots, every API, 17 compiled examples incl. 6 RTOS, worked Test-1-style answer, RTOS section 11, practice question). Rebuild with `py make_guide.py` |
| Example programs | `Guide_Examples/*.cpp` + ready `.bin` files in `Guide_Examples/bin` (compiled in `Mbed Programs\Guide_Build` via `build_guide.sh`) |
| Lecture PDFs | `Notes/` (same set as Test 1). For this board: `08a mbed Nucleo Timer Ticker.pdf`, end of `09 ADC.pdf` (AnalogIn), end of `EIOTS_10 Sept2026_UART.pdf` (mbed Serial, two-mbed TX/RX) |
| Ready Mbed program | `C:\Users\TUF FA707RC-HX024W\Mbed Programs\Test2` (shortcut in this folder). Already built once, so the next build only compiles `main.cpp` |
| Command-line build | `C:\Users\TUF FA707RC-HX024W\Mbed Programs\build_test2.cmd` → `Test2\BUILD\LPC1768\ARMC6\Test2.bin` |
| Past LPC1768 work to copy from | `08 Assessment 3/Task3B` (3 Tickers), `11 Mbed Assignment 1` (pots + LCD bars + PWM LEDs), `13 Assignment 1` (two boards over UART p9/p10), `14 Assignment 2` (pots → serial, LED count, LCD bar + %), `12 Lab 7/Mbed_CamLCD` (PC → board over USB serial) |

---

## 0. Test-day checklist

1. **Plug in the board** and check that a USB drive appears with `MBED.HTM` on it (it may have no "MBED" label; on this laptop it is usually `E:`).
2. **Open the `Test2` program** in Mbed Studio (or let Claude build it with `build_test2.cmd`). Target = LPC1768.
3. **Flash the board check first:** copy `Test2.bin` to the MBED drive, press the reset button on the mbed module. You should see "Test 2 ready", pot values and the joystick on the LCD, LED1 blinking. That proves the board, LCD and flashing all work before you spend time on the question.
4. **Give Claude the question.** Paste or photograph the whole paper and say:
   *"LPC1768 + mbed Application Board, Mbed OS 6, program `C:\Users\TUF FA707RC-HX024W\Mbed Programs\Test2`, replace main.cpp, use lcd_c12832.h for the LCD, build with build_test2.cmd and copy the .bin to the MBED drive."*
5. **Check the code against the traps below** (Ticker callbacks only set flags, no `count` variable name, no p5–p8/p11).
6. **Build, flash, press reset, test every task on the board.** If something is wrong, describe what the board does, not just "it doesn't work".
7. Keep a header comment saying what the program does and which pins it uses (marked last time).
8. After the test: copy `main.cpp` + the `.bin` into this folder (like `09 Test 1/NUC140`).

### Traps found in the labs (all of these happened before)
- **Never** use `printf`, `BusOut`, `BusIn`, `lcd.update()` or anything with a mutex inside a `Ticker` / `Timeout` / `InterruptIn` callback → Mbed OS fatal error, board freezes or the LEDs flash the error pattern. In the callback, change a `volatile` variable or a `DigitalOut`, then do the rest in `main`.
- Mbed OS 6 has **no `wait()`** and **no `Serial` class**. Use `ThisThread::sleep_for(200ms)`, `wait_us()`, and `printf` / `BufferedSerial`.
- Times are chrono: `ticker.attach(&fn, 500ms)` (not `0.5`), `timer.elapsed_time() >= 200ms` (not `read_ms()`).
- `printf("%f")` prints nothing useful by default → print integers (`mv / 1000`, `mv % 1000`).
- Don't name a global `count` (clashes with `std::count`) → use `ticks`, `num`, etc.
- **p5, p6, p7, p8, p11 are the LCD.** The lecture's `DigitalOut(p5)` / `(p6)` example breaks the LCD → use LED1–LED4.
- p13/p14 (UART1) and p27/p28 (UART2) are taken by the joystick and I2C on the app board → for a second UART use **p9 TX / p10 RX** (UART3).
- Mbed Studio "Run" can hang at "waiting for debug probe" → just copy the `.bin` and press reset.
- On the older board (no COM port) `sleep_for` woke only once per second → pace loops with a `Timer` busy-wait instead.
- A pot reading stuck at a fixed value or 0 → re-seat the LPC1768 in the app-board socket.

---

## 1. Board pins (LPC1768 + Application Board MSI-0315B)

| Device | Pin | Notes |
|---|---|---|
| LED1–LED4 | on the mbed module | `DigitalOut`, **1 = on** |
| RGB LED | R p23, G p24, B p25 | **0 = on**, all three can be `PwmOut` |
| Joystick | down p12, left p13, centre p14, up p15, right p16 | **1 = pressed**, use `PullDown` |
| Pot 1 / Pot 2 | p19 / p20 | `AnalogIn`, 12-bit ADC, 3.3 V |
| Speaker | p26 | `PwmOut` (period = 1 / frequency, duty 0.5) |
| LCD 128x32 (C12832) | MOSI p5, RESET p6, SCK p7, A0 p8, nCS p11 | `lcd_c12832.h` |
| Accelerometer MMA7660 / temperature LM75B | I2C SDA p28, SCL p27 | not in the lectures |
| XBee socket | TX p9, RX p10 | also the free UART3 |
| Analog out | p18 | `AnalogOut` (DAC, 10-bit) |
| USB serial to PC | USBTX / USBRX | 115200 baud (`mbed_app.json`) |

There is **no 7-segment display, no keypad and no push button** like the NUC140. Expect the Test 1 ideas to move to this board like this:

| Test 1 (NUC140) | Likely on the LPC1768 |
|---|---|
| Timer1 countdown on the 7-segment | Ticker / Timer countdown shown as a number on the LCD (or LED1–4 in binary) |
| Keypad +/- changes the running-LED speed | Joystick up/down changes the Ticker period of LED1–4 running lights |
| Timer2 bar grows on the LCD | Ticker grows a bar on the 128x32 LCD (only 32 px high) |
| Push button adds boxes | Joystick centre (`InterruptIn` or edge-detected `DigitalIn`) adds boxes beside the bar |
| Variable resistor | POT1 / POT2 controls a speed, a bar, the RGB brightness or the speaker pitch |
| UART | USB serial to the PC, or two boards over p9/p10 |

---

## 2. Mbed OS 6 API cheat sheet

```cpp
#include "mbed.h"
#include "lcd_c12832.h"

DigitalOut led1(LED1);
DigitalIn  joy_up(p15, PullDown);	// 1 = pressed
AnalogIn   pot1(p19);
PwmOut     red(p23);			// RGB, 0 = on
PwmOut     spk(p26);
LCD_C12832 lcd;				// call lcd.init() at the start of main
```

### Timer (polling, "scheduled programming", lecture 08a Ex 1/2)
```cpp
Timer t1, t2;
t1.start(); t2.start();
while (true) {
	if (t1.elapsed_time() >= 200ms)  { task1(); t1.reset(); }	// lecture: t1.read_ms() >= 200
	if (t2.elapsed_time() >= 1000ms) { task2(); t2.reset(); }
}
```
`start()`, `stop()`, `reset()`, `elapsed_time()` (chrono). Microseconds: `duration_cast<microseconds>(t.elapsed_time()).count()`.

### Ticker (interrupt, lecture 08a Ex 3)
```cpp
Ticker tk1;
volatile int ticks = 0;
void on_tk1() { ticks++; led1 = !led1; }	// short, no printf / LCD
tk1.attach(&on_tk1, 200ms);			// lecture: attach(&flip1, 0.2)
tk1.detach();					// stop
// change the rate (e.g. from the joystick): detach, then attach with the new period
```
`attach_us(fn, us)` exists in older mbed; in OS 6 just pass `std::chrono::microseconds(us)`.

### Timeout (fires once) and InterruptIn (event-driven)
```cpp
Timeout once;  once.attach(&fn, 2s);
InterruptIn centre(p14);  centre.mode(PullDown);  centre.rise(&on_press);	// fall() for release
```
Joystick buttons bounce → in `on_press` ignore presses within ~200 ms (check a `Timer`), or use polling with `last` edge detection in `main`:
```cpp
bool now = joy_centre;  if (now && !last) { boxes++; }  last = now;
```

### LCD (`lcd_c12832.h`, our own driver, 128 x 32 px)
```cpp
lcd.init();				// once
lcd.clear();				// clear the buffer
lcd.text(x, y, "Hello", scale);		// 5x7 font, 6 px per char → 21 chars per line, y = 0 / 11 / 22 for 3 lines
lcd.pixel(x, y, on);
lcd.box(x, y, w, h);			// outline
lcd.fill_box(x, y, w, h, on);		// filled (on = false erases)
lcd.update();				// send buffer to the LCD — only from main, never from a Ticker
```
Pattern: draw everything into the buffer, then one `update()` per loop. A vertical bar growing upward: `lcd.fill_box(0, 32 - h, 8, h);` with `h` 0–32.

### AnalogIn (ADC, lecture 09 end)
```cpp
float f = pot1.read();			// 0.0 ~ 1.0 (x 3.3 V)
uint16_t raw = pot1.read_u16() >> 4;	// 0 ~ 4095 (12 bits)
uint32_t mv = raw * 3300 / 4095;	// millivolts
```
Lecture version (mbed 2): `pc.printf("%f \n\r", Ain.read()); wait(0.5);` → in OS 6 use `printf` with integers and `ThisThread::sleep_for(500ms)`.

### PwmOut (RGB brightness, speaker)
```cpp
red.period_ms(10);  red = 1.0f - level;		// active low: 1.0 = off
spk.period(1.0f / 440);  spk = 0.5f;		// 440 Hz tone, spk = 0 for silence
```

### Serial (lecture 10 end)
```cpp
printf("POT1 = %d\r\n", raw);			// USB serial, 115200 from mbed_app.json

BufferedSerial pc(USBTX, USBRX, 115200);	// raw bytes
pc.set_blocking(false);
char c;  if (pc.read(&c, 1) == 1) { ... }
pc.write("Hi\r\n", 4);

BufferedSerial link(p9, p10, 9600);		// board-to-board: TX p9 → other RX p10, and GND-GND
```
Lecture API (mbed 2 `Serial`): `baud`, `format`, `putc`, `getc`, `printf`, `scanf`, `readable`, `writeable`, `attach`. The LPC1768 has 4 UARTs: 3 on pins (p9/p10, p13/p14, p28/p27), the 4th goes to the USB link (`USBTX`, `USBRX`).

### Extra, not in the lectures (I2C sensors on the app board)
- LM75B temperature, 8-bit address 0x90: read 2 bytes from register 0, `t = ((b0 << 8) | b1) >> 5`, °C = t × 0.125 (signed 11-bit).
- MMA7660 accelerometer, 8-bit address 0x98: write `{0x07, 0x01}` to switch it on, then read registers 0x00–0x02 (X, Y, Z, 6-bit signed, about 21 counts per g).
```cpp
I2C i2c(p28, p27);
char reg = 0, d[2];
i2c.write(0x90, &reg, 1);  i2c.read(0x90, d, 2);
```

---

## 3. Theory from lecture 08a (short-answer questions)

- Timing and events: a video camera must capture frames at exact intervals; a car must react at once to a crash sensor (airbag).
- **Interrupts** stop the running code so higher-priority code runs. They can be **timed** (Ticker) or **external / event-driven** (InterruptIn, mouse click).
- The simplest interrupt just increments a counter at a fixed rate in the background; main code checks the counter → **scheduled programming** (e.g. some tasks every 10 ms, others every 100 ms).
- **Timer object** (polling): main code keeps reading the time and calls the task when it is due.
- **Multiple rates:** engine spark / injection every ~1 ms (up to 8000 rpm) vs fuel level every 1000 ms; no point running both at the same rate.
- **Challenges:** each task must finish within its period or the timing overruns and goes out of sync; the amount of code sets how fast the CPU must be; tasks may need priorities (a 1 ms and a 100 ms task are both due every 100 ms); **no wait / delay (polling) inside a scheduled design**.
- **Ticker object:** a recurring interrupt calls the attached function at a set rate. Advantage over Timer: main doesn't read the time, so it is free to run other code.
- Ticker functions: `attach` (seconds), `attach_us` (microseconds), member-function versions, `detach`.
- LPC1768 facts: Cortex-M3, 96 MHz, 512 KB flash, 64 KB RAM, ARMv7-M (Thumb-2), 3-stage pipeline (fetch, decode, execute), Harvard architecture. 12-bit ADC (8 channels, 6 on mbed pins p15–p20), 10-bit DAC (p18), 6 PWM (p21–p26), 4 UARTs, 2 SPI, 2 I2C.

---

## 4. Build and flash

**Mbed Studio:** open `Test2` → target LPC1768 → Build (hammer). Output `BUILD\LPC1768\ARMC6\Test2.bin`.

**Command line** (Claude can run this):
```bash
cmd //c "C:\Users\TUF FA707RC-HX024W\Mbed Programs\build_test2.cmd"
```
It writes `Test2\build.log` and prints the errors and the image size.

**Flash:** copy `Test2.bin` to the MBED drive (the newest `.bin` on the drive runs), then press the reset button on the mbed module. On the board with a COM port, sending a serial break also resets it.

**Serial monitor:** Mbed Studio's serial monitor or Tera Term at 115200 on the board's COM port (was COM9 on this laptop).
