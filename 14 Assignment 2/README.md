# Assignment 2 — LPC1768 ADC → serial, LEDs, LCD bar

## Question

Develop a program on the LPC1768:

1. Transfer or serially connect to a Serial Port Monitor on PC
2. Transfer the ADC readings on 2 potentiometers on Applications board to serial port monitors
3. Control the number of LEDs (4) on the LPC applications board
   * 1 LED (<25% of ADC)
   * 2 LED (<50% of ADC)
   * 3 LED (<75% of ADC)
   * 4 LED (>75% of ADC)
4. Display on LCD a graphical bar that corresponds to the ADC reading.
5. Display on LCD display also the percentage reading of the ADC being measured.

## Solution

| | |
|---|---|
| Mbed program | `C:\Users\TUF FA707RC-HX024W\Mbed Programs\Asg2_Pots_LED` (shortcut in this folder) |
| Files here | `main.cpp`, `lcd_c12832.h`, `font5x7.h`, `mbed_app.json`, `Asg2_Pots_LED.bin` |
| Flash | copy `Asg2_Pots_LED.bin` to the MBED drive, press the reset button |

| Task | How |
|---|---|
| 1 Serial | USB serial 115200 baud (`mbed_app.json`). Open Mbed Studio serial monitor / Tera Term on the board's COM port |
| 2 ADC → PC | POT1 (p19) and POT2 (p20), 12-bit (0–4095), 8 readings averaged, one line every 0.5 s |
| 3 LEDs | LED1–LED4 on the LPC1768 module: <25 % = 1, <50 % = 2, <75 % = 3, ≥75 % = 4 |
| 4 Bar | one bar per pot on the 128x32 LCD (0–126 px = 0–100 %), ticks under the bar at 25/50/75 % |
| 5 Percent | each LCD line shows `>POT1 2048 1.65V  50%` |

**Which pot drives the LEDs:** the one turned last (a move of more than ~1.5 %), so both pots control the LEDs.
The joystick can also pick: left = POT1, right = POT2, centre = swap. POT1 is used at power-up.
The `>` on the LCD and `POT1 -> LEDs 3` on the serial monitor show which one.

Serial output example:

```
POT1 = 2048 ( 50%) 1.65V | POT2 = 1000 ( 24%) 0.80V | POT1 -> LEDs 3
```

**Built + flashed 2026-10-29:** 0 errors, 35 KB, on the E: board (COM9). Checked on the board: serial every 0.5 s, LED count follows whichever pot is turned, LCD bars + %.
If a knob seems stuck (fixed value or exactly 0), re-seat the LPC1768 in the app-board socket.
