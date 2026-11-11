# Test 1 Study Guide (RBB3023 Embedded & IoT Systems)

Lecturer: Patrick Sebastian. Tests are worth 30% of the course.
LLM coding is allowed in the test, so this guide does two jobs:
1. It is a fast reference for theory and for spotting wrong code.
2. It lists what to tell Claude so the code comes out right the first time (section 0).

The test uses one of these **two boards** (confirmed), so every hardware topic is covered for both:

| | Board A: Nu-LB-NUC140 (red Nuvoton board) | Board B: mbed NXP LPC1768 (+ Application Board) |
|---|---|---|
| Core | Cortex-M0, up to 50 MHz, 128 KB flash, 16 KB SRAM | Cortex-M3, 96 MHz, 512 KB flash, 64 KB RAM |
| Tool | Keil MDK (uVision), C, NUC140 BSP `Drv*` drivers + registers | Mbed Studio, C++, Mbed OS 6 API |
| Flash | Keil: Rebuild (F7), then Download (F8) via Nu-Link | Build, copy `.bin` to the `MBED` drive, press reset |
| Lectures | 03, 04, 05, 06 GPIO, 07 LCD, 08 Timer, 09 ADC, 10 UART | 08a mbed Timer/Ticker, end of 09 ADC (AnalogIn) |

(The course slides also name the STM32 Nucleo-F411RE, but it is not used in the test. Section 3.6 keeps a short backup note on it.)

The lecture PDFs are in `Notes/`, numbered in lecture order.

---

## 0. Test-day checklist (do this first)

1. **Find out which board you have.** Red board with an LCD and a 3x3 keypad means NUC140/Keil. Small blue mbed module on the Application Board (with the small LCD and joystick) means LPC1768/Mbed Studio.
2. **Open a working project; don't start from scratch.**
   - NUC140: copy a whole lab folder (for example `08 Assessment 3/Task3A`, or `05 Assessment 2/Task2A` if you need UART) and replace `main.c`. The Keil project already has the BSP paths set. Build once before you change anything.
   - LPC1768: in Mbed Studio, open an existing program (`Lab5_Timer`, `Task3B_Ticker`) or make a new one and copy `main.cpp`. `lcd_c12832.h` + `font5x7.h` are the LCD driver.
3. **What to paste to Claude** (this is what gets correct code on the first try):
   - the full question text
   - the board, with "NUC140 Keil BSP" or "LPC1768 Mbed OS 6"
   - the time formula needed (timer period), the pins or devices used, and what the LCD, 7-segment and serial output should show
   - for NUC140: "use DrvGPIO / register-level Timer like the lecture, no TIMER.c". For Mbed: "Mbed OS 6, no printf or mutex in Ticker callbacks".
4. **Check the generated code against sections 2 and 3** (pins active-low? correct formula? flag cleared in the ISR?).
5. **Build. If errors, paste the full error output back to Claude.**
6. Write a header comment that says what the program does and which pins it uses (lecturers grade this too).

### Common traps (learned in the labs)
- NUC140: `DrvSYS_Delay(us)` is limited to about **349 ms per call** at 48 MHz (24-bit SysTick). Use a loop for longer delays.
- NUC140: never add the BSP `TIMER.c` when you write your own `TMR0_IRQHandler` (duplicate symbol).
- NUC140: Keil MDK-Lite refuses images over **32 KB** (error L6047U). Keep big bitmaps and tables small.
- NUC140: Keil reports exit code 1 when there are warnings (the BSP LCD.c/SPI.c always gives 2). **0 errors means it built.**
- NUC140: always clear the interrupt flag in the ISR (`TIMER0->TISR.TIF = 1;`, `ADC->ADSR.ADF = 1;`), or the ISR fires forever.
- Mbed OS 6: no `printf`, `BusOut`, or mutex inside `Ticker` / `Timeout` / `InterruptIn` callbacks (MbedOS fatal error). Set a `volatile` flag in the callback and print in `main`.
- Mbed OS 6 has no `wait()`. Use `ThisThread::sleep_for(200ms)` or `wait_us()`.
- Mbed: a global named `count` clashes with `std::count`, so name it `ticks` or similar.
- LPC1768 + Application Board: **p5–p8 and p11 belong to the LCD.** The lecture's `DigitalOut(p5)` example breaks the LCD, so use LED1–LED4 instead.
- Mbed Studio flashing can hang at "waiting for debug probe". Copy the `.bin` from `BUILD/LPC1768/ARMC6/` to the MBED drive and press reset. The newest `.bin` on the drive runs.

---

## 1. Theory (lectures 01–05)

### 1.1 Embedded system / firmware / IoT
- **Embedded system:** a computer (processor + memory + I/O) with a *dedicated* function inside a larger mechanical or electrical system. A general-purpose PC, by contrast, handles many kinds of task.
- **Firmware:** low-level software stored in non-volatile memory (EEPROM/flash) that runs a single-purpose device. Usually written in assembly or C.
- **Embedded software:** software for a particular device, often C/C++, sometimes on an RTOS.
- **IoT:** an embedded system with a network/Internet connection.
- Example: a liquid level controller, manual first, then MCU-based, then an IoT version with the level reported online.

### 1.2 ARM
- ARM = Advanced RISC Machines (Cambridge, UK). ARM **licenses** core designs and does not make chips. Partners (Nuvoton, NXP, ST) fabricate them.
- Architecture profiles: **v7-A** (applications), **v7-R** (real-time), **v7-M** (microcontroller). Cortex-M0 is **v6-M** (Thumb only). Cortex-M3/M4 are v7-M (Thumb-2).
- Cortex-M3: **3-stage pipeline (Fetch, Decode, Execute)** and **Harvard architecture** (separate instruction and data buses, so fetch and data access can happen in parallel).
- Pipeline depths: ARM7 = 3, ARM9 = 5, ARM10 = 6, ARM11 = 8. ARM7 is Von Neumann; ARM9 and later are Harvard.
- **Processor core** (fetches and executes, e.g. ARM7TDMI) vs **MPU/MCU core** (core + cache, MMU, and tightly coupled blocks).

### 1.3 Registers (Cortex-M)
- 16 visible 32-bit registers: **R0–R12** general purpose, **R13 = SP** (stack pointer), **R14 = LR** (link register, return address of a subroutine call), **R15 = PC**.
- Classic ARM has 37 registers in total: 30 general, 1 PC, 1 CPSR, 5 SPSR. Banked copies are used in the exception modes.
- 7 modes: User (the only unprivileged mode), System, Supervisor (SVC, entered on reset), FIQ, IRQ, Abort, Undef.
- **CPSR:** N Z C V flags (bits 31–28), I and F (IRQ/FIQ disable, 1 = disabled), T (1 = Thumb), mode bits.

### 1.4 Condition flags (likely exam calculation)
| Flag | Set when |
|---|---|
| N | result bit 31 (bit 7 for 8-bit) = 1, i.e. negative |
| Z | result = 0 |
| C | **add:** the unsigned result doesn't fit (carry out), so C = 1. **sub:** C = 1 means *no borrow* (the answer is correct); C = 0 means a borrow happened |
| V | signed result is wrong: (+)+(+) gives (−), or (−)+(−) gives (+) |

8-bit worked examples from the slides:
| Operation | Binary result | C | V | Why |
|---|---|---|---|---|
| 3 + 9 | 0000 1100 | 0 | 0 | fits |
| 127 + 115 | 1111 0010 | 0 | 1 | 242 fits as unsigned; as signed, + + + gives − |
| 143 + 211 (−113 + −45) | 0110 0010 | 1 | 1 | 354 > 255; − + − gives + |
| 63 + 211 (63 + −45) | 0001 0010 | 1 | 0 | 274 > 255; the signed result 18 is correct |
| 228 + 156 (−28 + −100) | 1000 0000 | 1 | 0 | 384 > 255; −128 is a valid signed result |
| 255 + 5 | 0000 0100 | 1 | – | 260 − 256 = 4 |
| −100 − 100 | | | 1 | −200 < −128 |

Ranges: unsigned 8-bit is 0–255; signed (2's complement) is −128 to +127 (0x80 = −128, 0xFF = −1).

### 1.5 Memory
- Byte addressable, 32-bit address space (0x0000_0000 to 0xFFFF_FFFF). A word is 32 bits, a half-word 16, a byte 8.
- Load/store architecture: only LDR/STR access memory.
- Alignment: word addresses are multiples of 4, half-word addresses multiples of 2.
- **Little endian** (ARM's default; x86 too): the least significant byte goes at the lowest address. The word 0x30201000 at address 0 is stored as 00, 10, 20, 30 at addresses 0–3. **Big endian** (68k, SPARC) is the reverse.
- Hierarchy: registers (fastest, smallest) → cache → RAM → disk (slowest, biggest, cheapest).
- **NUC140 memory map:** APROM (flash) at 0x0000_0000–0x0001_FFFF (128 KB); LDROM (4 KB ISP loader) at 0x0010_0000; SRAM at 0x2000_0000–0x2000_3FFF (16 KB); APB peripherals at 0x4000_0000; AHB peripherals at 0x5000_0000; Cortex-M0 system registers at 0xE000_E000.
  - Examples: `GPIO_BASE` = AHB + 0x4000 = 0x5000_4000; GPIOB = GPIOA + 0x40, GPIOC = +0x80, GPIOD = +0xC0, GPIOE = +0x100 (`PORT_OFFSET` = 0x40). TIMER0 = 0x4001_0000, UART0 = 0x4005_0000, ADC = 0x400E_0000.

### 1.6 CMSIS (lecture 04)
- Cortex Microcontroller Software Interface Standard, defined by ARM with the vendors. Its goal is software reuse and portability across Cortex-M chips.
- Three layers: **Core Peripheral Access Layer** (ARM: NVIC, SysTick, core registers), **Device Peripheral Access Layer** (vendor: register structs and addresses), and optional **access functions for peripherals**.
- Files: ARM provides `core_cm0.h` and `core_cm0.c`. Nuvoton provides `NUC1xx.h` (IRQ numbers, register structs, base addresses), `startup_NUC1xx.s` (vector table, Reset_Handler), and `system_NUC1xx.c` (`SystemInit`, clock).
- Coding rules: `<stdint.h>` types (`uint8_t` ...). Exception handlers end in `_Handler` and interrupt handlers in `_IRQHandler`. IRQ numbers end in `_IRQn`. Default handlers are weak, endless loops.
- NUC140 IRQ numbers: BOD 0, WDT 1, EINT0 2, EINT1 3, GPAB 4, GPCDE 5, PWMA 6, PWMB 7, **TMR0 8, TMR1 9, TMR2 10, TMR3 11, UART02 12, UART1 13**, SPI0–3 14–17, I2C0/1 18–19, … **ADC 29, RTC 31**. Core exceptions: SysTick −1, PendSV −2, SVCall −5, HardFault −13, NMI −14.

### 1.7 Keil project from scratch (lecture 05)
New uVision project in a `Keil` folder → device **NUC140VE3CN** → do **not** copy `startup_NUC1xx.s`. Make three groups:
- **CMSIS:** `core_cm0.c`, `system_NUC1xx.c`, `startup_NUC1xx.s`
- **Library:** `GPIO.c`, `SYS.c`, `SPI.c` (+ `UART.c` if needed)
- **Source:** `main.c`, `LCD.c`, `Seven_Segment.c`, `Scankey.c`

Then set these options:
- Output: `obj` folder. Listing: `lst` folder.
- User: `fromelf --bin ".\obj\@L.axf" --output ".\obj\@L.bin"`
- C/C++: include paths for CMSIS, Device, NUC1xx and NUC1xx-LB_002.
- Linker: `--first='startup_NUC1xx.o(RESET)' --entry Reset_Handler`
- Debug: Nu-Link.

### 1.8 C recap (lecture 02)
- `x++` vs `++x`: `y = x++` gives y = 5, x = 6; `y = ++x` gives both 6.
- `=` vs `==`: `if (x = 6)` is always true.
- `&` vs `&&`: `1 & 2` = 0, but `1 && 2` = true.
- Integer division: `5/10` = 0, `5/10.0` = 0.5. Division by 0 is a fault.
- Arrays: `x[3] == *(x+3)`, indexes run 0..N−1, the array name is the address of element 0, and a string ends with `'\0'`.
- Pointers: `&` = address of, `*` = value at. `void f(char *p){*p -= 32;}` changes the caller's variable.
- `struct` / `typedef struct {...} Tally;`, accessed with `.` (or `->` through a pointer).
- Sizes on ARM: char 1, int 4, float 4, int64_t 8 bytes. The compiler pads to word boundaries.
- Loops: `while` (0 or more runs), `do…while` (at least 1), `for`. `break` leaves the loop or switch; `continue` jumps to the next iteration.
- Bit operations (used everywhere for registers): set `reg |= (1<<n)`, clear `reg &= ~(1<<n)`, toggle `reg ^= (1<<n)`, test `(reg>>n) & 1`.
- `volatile` on any variable shared with an ISR.

---

## 2. Board A: Nu-LB-NUC140 (Keil, C)

### 2.1 Pin map
| Device | Pins | Active level |
|---|---|---|
| 4 red LEDs | GPC12, 13, 14, 15 | **0 = ON** |
| RGB LED | GPA12, 13, 14 | **0 = ON** (see note) |
| Buzzer | GPB11 | 0 = ON |
| 7-seg digit select | GPC4 (rightmost, digit 0) … GPC7 (leftmost, digit 3) | 1 = ON |
| 7-seg segments | GPE0–7 (c, dot, f, a, b, d, e, g) | 0 = ON |
| 3x3 keypad | GPA0–5 (columns GPA0–2, rows GPA3–5), quasi mode | `ScanKey()` returns 1–9, 0 = none |
| LCD 128x64 | SPI3: GPD8 CS, GPD9 CLK, GPD10/11 data, GPD14 backlight | |
| Variable resistor VR1 | GPA7 = ADC7 | 0–3.3 V |
| UART0 | GPB0 RX0, GPB1 TX0 (to the RS232 DB9) | |
| UART1 | GPB4 RX1, GPB5 TX1 (also wired to LIN) | |
| UART2 | GPD14 RX2, GPD15 TX2 | |
| SW_INT button | GPB15 (INT1 / T0EX) | |
| Headers | JP4 = GPIOA, JP5 = GPIOB, JP6 = GPIOC, JP7 = GPIOD, JP8 = GPIOE | |

Note on the RGB colours: the GPIO lecture says GPA12 = Blue, 13 = Green, 14 = Red. The board-intro schematic slide says the opposite (14 = Blue, 12 = Red). Your labs used **12 = B, 13 = G, 14 = R**. If the colour comes out wrong on the day, swap 12 and 14.

Board supply is +5 V. The USB port gives at most 500 mA. A short circuit can make the PC cut USB power.

### 2.2 Program skeleton
```c
#include <stdio.h>
#include "NUC1xx.h"
#include "SYS.h"
#include "GPIO.h"
#include "LCD.h"
#include "Seven_Segment.h"
#include "Scankey.h"

int32_t main(void)
{
	UNLOCKREG();
	DrvSYS_Open(48000000);		// 48 MHz from PLL (12 MHz crystal)
	LOCKREG();
	// or HCLK = 12 MHz crystal (lecture timer/ADC samples):
	// SYSCLK->PWRCON.XTL12M_EN = 1; DrvSYS_Delay(5000); SYSCLK->CLKSEL0.HCLK_S = 0;

	init_LCD(); clear_LCD();
	OpenKeyPad();
	DrvGPIO_Open(E_GPC, 12, E_IO_OUTPUT); DrvGPIO_SetBit(E_GPC, 12);	// LED off
	while (1) { }
}
```

### 2.3 GPIO (lecture 06)
- Modes (PMD register, 2 bits per pin): **00 input** (high impedance), **01 push-pull output**, **10 open-drain**, **11 quasi bi-directional** (the reset default; used for the keypad).
- `DrvGPIO_Open(E_GPC, 12, E_IO_OUTPUT)`: other modes are `E_IO_INPUT`, `E_IO_OPENDRAIN`, `E_IO_QUASI`.
- `DrvGPIO_SetBit(port, pin)` drives 1; `DrvGPIO_ClrBit` drives 0; `DrvGPIO_GetBit` reads; `DrvGPIO_SetPortBits(E_GPA, 0x1234)` writes the whole port; `DrvGPIO_GetPortBits`.
- Inside the driver: `GPIOx->DOUT |= (1<<pin)`, `GPIOx->PIN` for reading, and the port address = GPIOA + port × 0x40.
- Macros: `#define LED0_ON DrvGPIO_ClrBit(E_GPC,12)`.
- **GPIO interrupt:**
  ```c
  DrvGPIO_Open(E_GPB, 15, E_IO_INPUT);
  DrvGPIO_EnableInt(E_GPA, 15, E_IO_RISING, E_MODE_EDGE);	// or E_IO_FALLING / E_IO_BOTH_EDGE, E_MODE_LEVEL
  DrvGPIO_SetDebounceTime(5, 1); DrvGPIO_EnableDebounce(E_GPA, 15);
  DrvGPIO_SetIntCallback(GPAB_callback, GPCDE_callback);	// callback(uint32_t GPA_status, uint32_t GPB_status)
  DrvGPIO_EnableEINT1(E_IO_FALLING, E_MODE_EDGE, eint1_callback);	// GPB15 SW_INT
  ```
  Edge-triggered = fires on a transition; level-triggered = fires while the line holds a level.
- **7-segment:** `show_seven_segment(digit 0~3, value 0~9)`, `close_seven_segment()`. To show a 4-digit number, multiplex: for each digit call `close_seven_segment()`, then `show_seven_segment(n, d)`, then `DrvSYS_Delay(5000)`.
- **Keypad:** `OpenKeyPad(); k = ScanKey();`. Use edge detection so one press acts once: `if (k && k != last_key) {...} last_key = k;`.
- Sensors from the lecture, all on a plain GPIO input: TCRT5000 IR on GPA0, PIR on GPA0 (0 = detected), relays, LCM16x2 (GPE data + GPA0–2), MAX7219 8x8 matrix (GPA0–2).

### 2.4 LCD (lecture 07)
- 128x64, SPI3 at 25 MHz. Text uses 4 lines of 16 characters (8x16 font): `print_Line(0~3, "text")`, `printS(x, y, str)`, `printS_5x7(x, y, str)`, `printC(x, y, ch)`, `clear_LCD()`.
- Graphics: `draw_Pixel(x, y, FG_COLOR, BG_COLOR)` (passing FG = BG_COLOR erases), `draw_LCD(buffer[1024])` draws a whole frame, `draw_Bmp8x8/16x16/32x32/64x64(x, y, fg, bg, bmp)`.
- Buffer layout: `buffer[x + (y/8)*128]`, bit `y%8`. The screen is 8 pages of 8 rows each.
- 2D drawing (2DGraphics.c): `LineBresenham(x1,y1,x2,y2,fg,bg)`, `CircleBresenham(xc,yc,r,fg,bg)`, `RectangleDraw` / `RectangleFill(x0,y0,x1,y1,fg,bg)`, `Triangle(...)`.
- Animation loop: draw, delay, erase (redraw with BG_COLOR), check the boundary and flip the direction, then x += dirX·movX.
- Games: draw, delay, erase, change X/Y, boundary check, collision detection. Collision can be direct (compare objects) or use a map array.
- Text with numbers: `char T[17]; sprintf(T, "ADC:%4d", v); print_Line(1, T);`. Pad to 16 characters so old text gets overwritten.

### 2.5 Timer (lecture 08), very likely on the test
- **4 timers (TIMER0–3), 24-bit up counters, 8-bit prescaler, 5 clock sources.**
- **Timeout = (1 / f_clk) × (PRESCALE + 1) × TCMPR.** PRESCALE is 0–255; TCMPR is 0–16,777,215.
- Modes: **0 one-shot** (interrupt once, then stop), **1 periodic** (interrupt, then restart from 0), **2 toggle** (periodic and the TOUT pin toggles), **3 continuous** (keeps counting through TCMPR, overflows at 2²⁴−1).

| Want (12 MHz source) | PRESCALE | TCMPR |
|---|---|---|
| 1 s (lecture) | 255 | 46875 → 1/12M × 256 × 46875 = 1 s |
| 1 µs tick | 11 | 1 count = 1 µs, so TCMPR = µs wanted |
| 100 ms | 11 | 100000 |
| 500 ms | 11 | 500000 |
| 1 ms | 11 | 1000 |
| 10 ms | 119 | 1000 (10 µs tick) |
| max with prescale 11 | 11 | 16,777,215 µs ≈ 16.7 s |

```c
#define ONESHOT 0
#define PERIODIC 1
#define TOGGLE 2
#define CONTINUOUS 3

volatile uint32_t t0_count = 0;

void TMR0_IRQHandler(void)
{
	t0_count++;			// keep ISRs short, set flags
	TIMER0->TISR.TIF = 1;		// write 1 to clear flag
}

void InitTIMER0(void)
{
	SYSCLK->CLKSEL1.TMR0_S = 0;	// 1. clock = 12 MHz crystal
	SYSCLK->APBCLK.TMR0_EN = 1;	//    enable clock
	TIMER0->TCSR.MODE = PERIODIC;	// 2. mode
	TIMER0->TCSR.PRESCALE = 255;	// 3. time out = 1/12M * 256 * 46875 = 1 s
	TIMER0->TCMPR = 46875;
	TIMER0->TCSR.IE = 1;		// 4. interrupt
	TIMER0->TISR.TIF = 1;
	NVIC_EnableIRQ(TMR0_IRQn);
	TIMER0->TCSR.CRST = 1;		// 5. reset counter, start
	TIMER0->TCSR.CEN = 1;
}
```
For Timer1/2/3 swap in `TMR1_S`/`TMR1_EN`, `TIMER1`, `TMR1_IRQn`, `TMR1_IRQHandler`. TIMER2/3 use `TMR2_S` / `TMR3_S`.

- **Watchdog (WDT):** 18-bit counter. Clock options: 10 kHz (WDT_S = 3), HCLK/2048 (2), 32 kHz (1), 12 MHz (0). Interval `WTIS` = 0–7 gives 2⁴, 2⁶, 2⁸, 2¹⁰, 2¹², 2¹⁴, 2¹⁶, 2¹⁸ WDT clocks. At 10 kHz, WTIS = 5 is 2¹⁴/10k ≈ 1.6 s. Reset happens 1024 clocks after the interrupt if WTRE = 1. Kick it with `UNLOCKREG(); WDT->WTCR.WTR = 1; LOCKREG();`. WDT registers are protected, so UNLOCKREG first. Setup: `WTIS`, `WTRE`, `WTIF = 1`, `WTIE = 1`, `NVIC_EnableIRQ(WDT_IRQn)`, `WTE = 1`, `WTR = 1`.
- **RTC:** 32.768 kHz crystal (`XTL32K_EN`), BCD time and calendar. Unlock by writing **INIR = 0xA5EB1357** and **AER = 0xA965**. Registers: TLR (time load) = 0x00HHMMSS, e.g. 0x00093043 = 09:30:43; CLR (calendar) = 0x00YYMMDD; TAR/CAR (alarm); TSSR.HR24_HR12 = 1 for 24 h mode; RIER.AIER (alarm interrupt), RIER.TIER (tick interrupt); TTR (tick rate 1 to 1/128 s); `RTC_IRQn`. In 12 h mode, PM hours = hour + 0x20 (e.g. 13:00 is stored as 0x21).
- **Capture pins:** T0EX = GPB15, T1EX = GPE5, T2EX = GPB2, T3EX = GPB3. With TEXEN = 1 and RSTCAPSEL = 0, the counter value is captured into TCAP on an edge of TEX (used to measure a pulse width, e.g. the SR04 ultrasonic echo). With RSTCAPSEL = 1 the counter resets on the edge instead.

### 2.6 ADC (lecture 09)
- 12-bit SAR ADC, 8 channels ADC0–7 on GPA0–7, up to 700–800 kSPS. ADC clock ≤ 16 MHz. **Vref = 3.3 V on the board.**
- **V = raw × 3.3 / 4096** (0 → 0 V, 2048 → 1.65 V, 4095 → about 3.3 V). Millivolts: `mv = raw * 3300 / 4095`.
- SAR = sample & hold, then a comparator against a DAC, with the successive approximation register stepping bit by bit until end-of-conversion.
- Modes: single, single-cycle scan, continuous scan. Inputs: single-ended (8 channels) or differential (4 pairs: 0/1, 2/3, 4/5, 6/7).
- ADC clock = source / (ADC_N + 1). Source options: 12 MHz (0), PLL (1), 22 MHz (2).
```c
void InitADC(void)
{
	GPIOA->OFFD |= 0x00800000;		// disable digital input on GPA7
	SYS->GPAMFP.ADC7_SS21_AD6 = 1;		// GPA7 = ADC7
	SYSCLK->CLKSEL1.ADC_S = 2;		// 22 MHz
	SYSCLK->CLKDIV.ADC_N = 1;		// 22/2 = 11 MHz
	SYSCLK->APBCLK.ADC_EN = 1;
	ADC->ADCR.ADEN = 1;
	ADC->ADCR.DIFFEN = 0;			// single-end
	ADC->ADCR.ADMD = 0;			// single mode
	ADC->ADCHER.CHEN = 0x80;		// channel 7
	ADC->ADSR.ADF = 1;			// clear flag
	ADC->ADCR.ADST = 1;			// start
}
// in loop:
while (ADC->ADSR.ADF == 0);		// wait for conversion
ADC->ADSR.ADF = 1;			// clear
v = ADC->ADDR[7].RSLT;			// 0~4095
ADC->ADCR.ADST = 1;			// start next
```
A ready, tested version is in `06 Lab 4/main_timer_adc_backup.c`. The driver alternative is `DrvADC_Open(ADC_SINGLE_END, ADC_CONTINUOUS_OP, 0xFF, INTERNAL_HCLK, 1); DrvADC_EnableADCInt(cb, 0); DrvADC_StartConvert(); DrvADC_GetConversionData(7);` (needs ADC.c in the project).

### 2.7 UART (lecture 10)
- Frame: **start bit (0) → 5–8 data bits, LSB first → optional parity (even/odd/none) → 1 or 2 stop bits (1)**. Asynchronous means no clock line, so both sides must use the same baud rate.
- 3 UARTs with FIFOs: UART0 has 64 bytes, UART1 and UART2 have 16. Hardware flow control (CTS/RTS) on UART0/1. IrDA SIR and RS-485 9-bit mode (bit 9 = 1 marks an address byte).
- Baud rate (UART_CLK = 22.1184 MHz):
  - **Mode 0: UART_CLK / [16 × (A+2)]** (115200 → A = 10; 9600 → A = 142)
  - **Mode 1: UART_CLK / [(B+1)(A+2)]**, B ≥ 8
  - **Mode 2: UART_CLK / (A+2)**, A ≥ 3
- RS-232 / RS-422 / RS-485: 1 / 1 / 32 drivers, 1 / 10 / 32 receivers. RS-232 is full duplex and RS-485 half duplex. Distance about 15 m vs 1200 m. RS-232 uses ±5 to ±15 V; RS-485 is differential. On the DB9: pin 2 = RX, pin 3 = TX, pin 5 = GND.
- Board-to-board wiring: **TX ↔ RX crossed + common GND.**
```c
#include "UART.h"	// add UART.c to the project
void UART_INT_HANDLE(uint32_t u32IntStatus)
{
	uint8_t c;
	while (UART0->ISR.RDA_IF == 1) { DrvUART_Read(UART_PORT0, &c, 1); /* store c */ }
}
STR_UART_T p;
DrvGPIO_InitFunction(E_FUNC_UART0);		// GPB0/GPB1 (E_FUNC_UART1 = GPB4/5)
p.u32BaudRate = 9600;  p.u8cDataBits = DRVUART_DATABITS_8;
p.u8cStopBits = DRVUART_STOPBITS_1;  p.u8cParity = DRVUART_PARITY_NONE;
p.u8cRxTriggerLevel = DRVUART_FIFO_1BYTES;
DrvUART_Open(UART_PORT0, &p);
DrvUART_EnableInt(UART_PORT0, DRVUART_RDAINT, UART_INT_HANDLE);
DrvUART_Write(UART_PORT0, (uint8_t *)"Hello\r\n", 7);
```
- Lecture examples:
  - SRF04/US100 ultrasonic: send 0x55, read 2 bytes, distance (mm) = b0 × 256 + b1.
  - HC-05 Bluetooth: 9600 baud, pairing PIN 1234. Read 8 bytes and show them on the LCD, or send distance as ASCII digits with `0x30 + digit`.
- Working code from your labs: `05 Assessment 2/Task2A` (UART1 115200 + Python on the laptop) and `00 Testing` (two-board UART1 Pong).

---

## 3. Board B: mbed LPC1768 / Nucleo (Mbed Studio, C++, Mbed OS 6)

### 3.1 LPC1768 + Application Board pins
| Device | Pin | Notes |
|---|---|---|
| LED1–LED4 | on the mbed module | 1 = ON |
| RGB LED | R p23, G p24, B p25 | **0 = ON**, PwmOut capable |
| Joystick | down p12, left p13, centre p14, up p15, right p16 | 1 = pressed |
| Pot 1 / Pot 2 | p19 / p20 | AnalogIn |
| Speaker | p26 | PwmOut |
| LCD C12832 128x32 | MOSI p5, RESET p6, SCK p7, A0 p8, nCS p11 | use `lcd_c12832.h` |
| Accelerometer MMA7660, temperature LM75B | I2C SDA p28, SCL p27 | |
| XBee | TX p9, RX p10 | |
| Analog out | p18 | AnalogOut |
| USB serial | USBTX/USBRX | 115200 via `mbed_app.json` |

### 3.2 Timer (scheduled programming, lecture 08a)
```cpp
#include "mbed.h"
Timer timer1;
DigitalOut out1(LED1);
int main() {
	timer1.start();
	while (1) {
		if (timer1.elapsed_time() >= 200ms) {	// lecture: timer1.read_ms() >= 200
			out1 = !out1;
			timer1.reset();
		}
	}
}
```
Timer functions: `start()`, `stop()`, `reset()`, `elapsed_time()` (OS 6); `read()`, `read_ms()`, `read_us()` in the lecture's mbed 2.

### 3.3 Ticker (interrupt-driven)
```cpp
Ticker flipper1, flipper2;
DigitalOut led1(LED1), led2(LED2);
void flip1() { led1 = !led1; }			// runs in interrupt context
void flip2() { led2 = !led2; }
int main() {
	flipper1.attach(&flip1, 200ms);		// lecture: attach(&flip1, 0.2)
	flipper2.attach(&flip2, 1s);
	while (1) ThisThread::sleep_for(200ms);	// lecture: wait(0.2)
}
```
Ticker functions: `attach(fn, interval)`, `attach_us(fn, us)`, `detach()`. Also `Timeout` (fires once) and `InterruptIn btn(p14); btn.rise(&fn);`.

**Theory points (likely short-answer questions):**
- **Timer** = polling: main keeps reading the time. **Ticker** = interrupt: the function is called in the background, so main is free.
- Scheduled programming means running tasks at different rates. In a car, engine injection runs every 1 ms but the fuel level only every 1000 ms.
- **Every task must finish before its period ends**, or timing overruns and the system loses sync.
- A long or blocking task stalls the others, so you can't use `wait` or other polling delays inside scheduled tasks.
- Decide the priority when two tasks are due at the same moment.
- Interrupts: timed (Ticker) or external/event-driven (InterruptIn). Examples: a camera capturing frames at exact intervals, an airbag responding to a crash sensor.

### 3.4 AnalogIn, serial, PWM
```cpp
AnalogIn pot1(p19);			// Nucleo: AnalogIn ain(PA_0) / A0
float f = pot1.read();			// 0.0 ~ 1.0  (× 3.3 V)
uint16_t raw = pot1.read_u16();		// 0 ~ 65535
printf("%d mV\r\n", (int)(f * 3300));	// printf goes to USB serial (float printf off by default -> print int)
PwmOut red(p23); red.period_ms(10); red = 1.0f - level;	// active-low RGB
```
- The lecture's `Serial pc(USBTX, USBRX); pc.printf(...)` is mbed 2. In OS 6 use plain `printf` (set the baud rate in `mbed_app.json`) or `BufferedSerial pc(USBTX, USBRX, 115200);`.
- Plain `printf` does not print floats (`%f`) by default, so print scaled integers.

### 3.5 Building and flashing (what worked in your labs)
- Mbed programs live in `C:\Users\TUF FA707RC-HX024W\Mbed Programs\<name>`. The `mbed-os` folder there is a junction to the shared `C:\Users\TUF FA707RC-HX024W\mbed-os`.
- In Mbed Studio: pick the target (LPC1768), then Build. Copy `BUILD\LPC1768\ARMC6\<name>.bin` to the MBED drive and press the reset button.
- Claude can also build from the command line (the steps are in memory) if Mbed Studio misbehaves.
- Keep `.mbedignore` (it skips cellular, lora, nfc, ble and wifi; don't ignore nanostack or linking breaks).

### 3.6 Backup only: Nucleo-F411RE (not used in the test)
- STM32F411RE (Cortex-M4F, 100 MHz). One user LED: **LED1 = PA_5** (1 = ON). User button: **BUTTON1 = PC_13** (reads 0 when pressed). Arduino headers A0–A5 = PA_0, PA_1, PA_4, PB_0, PC_1, PC_0. USB serial through the ST-LINK virtual COM port (USBTX/USBRX).
- Target `NUCLEO_F411RE`. Flash by copying the `.bin` to the `NODE_F411RE` drive.
- The code is the same Mbed API with different pin names. There is no LCD or joystick unless external parts are wired.
- The ADC slide example `AnalogIn Ain(PA_0)` is for this board.

---

## 4. Quick self-test (answers below)
1. Timer0, 12 MHz source, prescale 255. Which TCMPR gives 0.5 s?
2. ADC raw = 1024 with Vref 3.3 V. What voltage is that?
3. 8-bit 0x7F + 0x01. What are N, Z, C, V?
4. Store 0x12345678 at address 0x100, little endian. Which byte is at 0x100?
5. UART mode 0, UART_CLK = 22.1184 MHz, 9600 baud. What is A?
6. Which GPIO mode does the keypad use, and why?
7. Why is a Ticker better than polling a Timer?
8. Name the 3 CMSIS layers and the 3 files Nuvoton provides.
9. The RTC TLR is to be set to 14:05:30. What value?
10. What does `TIMER0->TISR.TIF = 1;` do, and what happens if you forget it?

Answers:
1. 0.5 × 12M / 256 = **23437.5, so 23438** (or prescale 11 and TCMPR 500000 for an exact value).
2. 1024 × 3.3 / 4096 = **0.825 V**.
3. Result 0x80: **N = 1, Z = 0, C = 0, V = 1** (127 + 1 = 128 is a valid unsigned result but overflows as signed).
4. **0x78** (the least significant byte goes first).
5. 22118400 / (16 × 9600) − 2 = **142**.
6. **Quasi bi-directional:** a pin can drive low (row scan) and also be read as input with a weak pull-up.
7. The Ticker runs the function by interrupt at an exact interval, so the CPU is free and timing doesn't depend on how long the main loop takes.
8. Layers: Core Peripheral Access, Device Peripheral Access, Access Functions. Files: `NUC1xx.h`, `startup_NUC1xx.s`, `system_NUC1xx.c`.
9. **0x00140530** (BCD).
10. It clears the interrupt flag by writing 1. If you forget, the flag stays set and the ISR re-enters forever, which hangs the program.
