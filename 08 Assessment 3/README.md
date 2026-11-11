# Assessment 3 (Practical Tasks #3)

## Task 3A: Timer0 + Timer1 bars on the Nuvoton LCD (`Task3A/`)

- Board: Nu-LB-NUC140, Keil project `Task3A/Keil/Proj_New.uvprojx` (copied from Lab 4).
- HCLK is the 12 MHz crystal. Both timers use a prescale of 11, so the timer clock is 1 MHz (1 µs per count).

| Timer  | Mode     | TCMPR  | Period | Bar   | Growth            | Full after |
|--------|----------|--------|--------|-------|-------------------|------------|
| Timer0 | periodic | 100000 | 100 ms | Bar 1 | +3 px per period  | 3 s        |
| Timer1 | periodic | 300000 | 300 ms | Bar 2 | +3 px per period  | 9 s        |

- Each bar can grow to 90 px. A full bar stays on screen for one period and then goes back to 0. The two bars reset on their own, each on its own timer.
- The LCD shows the title, two framed bars with the labels "Bar 1" and "Bar 2", and on the bottom lines each timer's period and the bar length (`xx/90`).
- LED GPC12 toggles on each Timer0 interrupt and LED GPC13 on each Timer1 interrupt, so you can see both rates on the board too.
- Keypad speed control. Each press changes the timer period by 50 ms, within 50 to 1000 ms. The new period is written to TCMPR and shows on the bottom lines.
  - **1** = Bar 1 faster, **7** = Bar 1 slower (Timer0)
  - **3** = Bar 2 faster, **9** = Bar 2 slower (Timer1)
- To change the starting rates, edit `T0_MS`, `T1_MS`, `STEP` and `BAR_MAX` at the top of `main.c`.
- The variable-resistor bar version is kept in `Task3A/main_vr_bar_backup.c`.
- Build: Keil, Rebuild, then Download. It builds with 0 errors. The 2 warnings come from the BSP's LCD.c and SPI.c.

## Task 3B: 3 Tickers on the LPC1768 (`Task3B/`)

- Mbed program: `C:\Users\TUF FA707RC-HX024W\Mbed Programs\Task3B_Ticker` (open it in Mbed Studio). `Task3B/` keeps a copy of the source, the built `.bin` and a shortcut.

| Ticker   | Period  | LED  | Blink rate |
|----------|---------|------|------------|
| ticker1  | 200 ms  | LED1 | 2.5 /s     |
| ticker2  | 500 ms  | LED2 | 1 /s       |
| ticker3  | 1000 ms | LED3 | 0.5 /s     |

- Serial monitor at **115200 baud** (set in `mbed_app.json`):
  - one line for every change: time, ticker, LED ON/OFF and count
  - every 1 s, a rate line with the counts per second (T1 = 5, T2 = 2, T3 = 1) and the totals
- The application board LCD has one row per Ticker: `LED1  200ms n=12`, a box that copies the LED, and under it a bar that grows 2 px per tick (full at 118 px, then back to 0). The 3 bars grow at 3 different speeds.
- Each change line on the serial monitor also shows the bar length (`bar xxx/118`).
- Flash: copy `Task3B_Ticker.bin` to the MBED drive, then press the reset button.
