# Test 2 - mbed LPC1768 + Application Board (answer FAST)

When the user pastes a test question:
1. Write the full answer into `C:\Users\TUF FA707RC-HX024W\Mbed Programs\Test2\main.cpp`.
   First copy the old main.cpp to `D:\UTP\UG Y2S7\01 IoT\15 Test 2\answers\Q<n>_main.cpp` if it holds a previous answer.
2. Run `"C:\Users\TUF FA707RC-HX024W\Mbed Programs\build_flash_test2.cmd"` (builds, copies .bin to the MBED drive).
   If it fails, fix the errors and run it again until BUILD OK.
3. Reply short: what the program does, what the user should see on the board, "press reset".
   Also save the final answer to `answers\Q<n>_main.cpp`.

Code style: very few comments, only where really needed (e.g. active-low RGB, ISR flag). No header block.
Replies to the user: short, no long explanations.

Reuse code from `Code_Modules.txt` and `Guide_Examples\*.cpp` (all compile). The LCD driver is
`lcd_c12832.h` (already in the program): `LCD_C12832 lcd; lcd.init(); lcd.clear(); lcd.text(x,y,str); lcd.update();`
plus `pixel / line / box / fill_box / circle`. Text lines at y = 0, 11, 22.

## Pins
| Part | Pins |
|---|---|
| LED1-LED4 | active HIGH (1 = on) |
| RGB LED | R p23, G p24, B p25 - active LOW (0 = on) |
| Joystick | down p12, left p13, centre p14, up p15, right p16 - `DigitalIn(pin, PullDown)`, 1 = pressed |
| POT1 / POT2 | p19 / p20 (AnalogIn, `read_u16() >> 4` = 0-4095) |
| Speaker | p26 (PwmOut) |
| LCD C12832 | p5 MOSI, p6 RESET, p7 SCK, p8 A0, p11 nCS (handled by lcd_c12832.h) |
| I2C | SDA p28, SCL p27 - LM75B temp 0x90, MMA7660 accel 0x98 |
| UART to other board | p9 TX, p10 RX |
| PC serial | USBTX/USBRX, `printf` at 115200 (mbed_app.json) |

## Mbed OS 6.17 rules (must follow)
- Delays: `ThisThread::sleep_for(500ms);` or `wait_us(n)`. No `wait()`. No `Serial` class - use `BufferedSerial`.
- `printf` has no `%f` - print ints (e.g. `x*100` as `%d.%02d`).
- Ticker/Timeout/InterruptIn callbacks: only set flags / change DigitalOut. No printf, LCD, mutex there.
- Timeout/Ticker `attach(&f, 500ms)`. Timer: `elapsed_time() >= 500ms`.
- Do not name a global `count` (clashes with std::count) - use `counter`.
- All PwmOut share one period; LED1 and p26 share a PWM channel - do not PWM LED1 with the speaker.
- RTOS: `Thread t(osPriorityNormal, 2048); t.start(f);`, Mutex, Semaphore, EventFlags, Mail/Queue,
  EventQueue (`btn.rise(isr)` where isr calls `queue.call(f)`; `queue.event()` does not compile here).
- Edge detect buttons (press once, no repeat): `pressed = now && !last; last = now;`.

## Serial monitor
`py -m serial.tools.miniterm COMx 115200` (find COMx in Device Manager, "mbed Serial Port").
