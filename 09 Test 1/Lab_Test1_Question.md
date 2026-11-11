# RBB 3023 Embedded and IoT Systems: Lab Test #1

(Copied from the photos of the test paper `EIOTS_LabTest1_Sept26set4.docx`. Name / ID boxes left out.)

**Using the KEIL IDE software, write a program that completes the following tasks:**

| Task | INPUT | OUTPUTS |
|---|---|---|
| **1** | Using internal **Timer 1**, to generate an output on the seven segment display on the Nuvoton Learning Board.<br><br>Timer1 should have a **decrementing display from 5 to 77** on the 7 segment display.<br><br>The timing should approximate a change in digit **every 0.5 seconds**. | **7 segment display Output**<br>(picture of the 4-digit 7-segment display) |
| **2** | **Keypad Input**<br><br>Input from the keypad should **increase/decrease the rate of the running LEDs on the MSB 7 segment display**.<br><br>(picture of the 3x3 keypad: an arrow from "**Increase delay**" points to the **top row, middle key**; an arrow from "**Decrease delay**" points to the **bottom row, middle key**)<br><br>*Hint: use normal polling method / permanent loop | **7 Segment Display**<br>The Display on the 7-segment display should:<br>(picture of the MSB digit with segments labelled a top, b top-right, c bottom-right, d bottom, e bottom-left, f top-left)<br>- The MSB display segments 'a', 'b', 'c', 'd', 'e' and 'f' **rotate counter clockwise / or blinks in a counter clockwise sequence**. |
| **3** | Using **Timer2** to generate a **display bar on the left side of the LCD** display on the Nuvoton Learning Board. The height of the display bar will change over time based on Timer2. **The bar should be higher over time.** | (picture: small bar labelled "Initial height", taller bar labelled "higher height") |
| **4** | Using **Push button**, generate or display a **number of boxes on the LCD** display on the Nuvoton learning board. Where the **initial number of boxes to appear are 0**. These boxes are to appear **beside the bar displayed on Task #3**. | (picture: the bar with 2 boxes beside it, labelled "**After 2 time pressed**") |

**Note: All tasks should work simultaneously for each change in the segment.**

## Marking scheme

Columns run 0 = Does not work, 1–2 = Individual parts work, 3 = Works to certain degree, 4 = Works (with glitch), 5 = Works perfectly.

| Item | Score range | Total |
|---|---|---|
| Task 1 Output | 0–5 | 5 |
| Task 2 Output | 0–10 (0 does not work, 2–4 individual parts work, 5–6 works to certain degree, 7–8 works with glitch, 9–10 works perfectly) | 10 |
| Task 2 Input Handling | 0–5 | 5 |

(The rest of the marking table is cut off in the photo.)

## Things to check in the wording

- **Task 1 "decrementing from 5 to 77":** the paper says *decrementing*, so the program counts **77 → 5**, then starts at 77 again. If the lecturer means counting up 5 → 77, it's a one-line change.
- **Task 2 keys:** from the arrows, top middle = increase delay and bottom middle = decrease delay. The code uses `ScanKey()` **1** (increase) and **7** (decrease), as changed by the user. **Confirmed on the board**: keys 1 and 7 are the top-middle and bottom-middle keys the arrows point to.
- **Task 2 direction:** the paper says counter clockwise, but the user confirmed it must be **clockwise**. The program rotates one segment a → b → c → d → e → f.
- **Task 4 "Push button":** the program uses the SW_INT button (GPB15).
