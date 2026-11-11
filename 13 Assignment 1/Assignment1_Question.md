# Assignment 1 — Two-board LPC1768 data transfer

## Question

Develop a program that will transfer the variable reading from one LPC1768
microcontroller to another LPC1768 microcontroller to display on:

1. LCD display of #2 LPC1768
2. Laptop that is connected to #2 LPC1768

## Solution

| | Board #1 (TX) | Board #2 (RX) |
|---|---|---|
| Mbed program | `C:\Users\TUF FA707RC-HX024W\Mbed Programs\Asg1_TX_Board1` | `...\Mbed Programs\Asg1_RX_Board2` |
| D: copy | `TX_Board1/` | `RX_Board2/` |
| Job | reads POT1 (p19) / POT2 (p20), sends a packet every 100 ms | receives, shows on LCD, prints to laptop |
| MBED drive (this laptop) | F: (old firmware, no COM port) | E: = COM9 |

**Wiring:** board #1 **p9 (TX)** -> board #2 **p10 (RX)**, plus **GND to GND**.
Leave the XBee sockets empty (p9/p10 are shared with them).

**Link:** UART3, 9600 baud, 8N1. Packet = text line `$seq,pot1,pot2,chk\n`,
chk = (seq + pot1 + pot2) % 256. Board #2 drops bad packets (`bad`), counts gaps
in seq (`lost`), and shows NO SIGNAL (LED4 on) after 1 s without a packet.

**Laptop:** board #2 prints one line per packet at 115200 on its USB serial port.
Live bar view: `py asg1_viewer.py` (or `py asg1_viewer.py COM9`).
Mbed Studio serial monitor / Tera Term also work.

**Notes:** Mbed OS 6 BufferedSerial (no `Serial` class). Board #2 reads
non-blocking in the main loop, so nothing runs inside an ISR.
Board #1 paces with a Timer busy-wait: `ThisThread::sleep_for(100ms)` only woke
once per second on that board (old interface firmware).
While nothing arrives, board #2 prints `NO SIGNAL  bytes N  bad M` every 1 s
(bytes 0 = check the p9 -> p10 wire and GND).

Board #2 does not count a board #1 reset (seq restarts at 0) as lost packets.

**Tested 2026-10-29:** 10 packets/s, lost 0, bad 0, shown on board #2 LCD + laptop (COM9).
POT1 and POT2 both follow the knobs (POT2 swept 1 ~ 4087). If a knob is stuck
(POT1 fixed ~1165 or POT2 exactly 0), re-seat the LPC1768 in the app-board socket.
Board #1 only needs power (laptop USB or a phone charger); only board #2 talks to the laptop.
