//
// Example 09 : UART between two LPC1768 boards (lecture 10, "two mbed" circuit)
//
//   The SAME program goes on both boards.
//   Wiring : board A p9 (TX) -> board B p10 (RX)
//            board A p10 (RX) <- board B p9 (TX)
//            GND <-> GND  (always join the grounds)
//   9600 baud, one line "P1=nnnn\n" every 0.5 s with this board's POT1
//   LCD line 1 = what this board sends, line 2 = what it received
//   Joystick centre on one board lights LED1 on the other board ("B1\n" / "B0\n")
//
//   p9/p10 = UART3 (XBee socket pins), the only free UART on the app board
//   (p13/p14 = joystick, p27/p28 = I2C sensors)
//
#include "mbed.h"
#include "lcd_c12832.h"

BufferedSerial link(p9, p10, 9600);	// TX, RX, baud
AnalogIn   pot1(p19);
DigitalIn  joy_centre(p14, PullDown);
DigitalOut led1(LED1);
LCD_C12832 lcd;
Timer t;

int main()
{
	char out[24], rx_line[24] = "", show[24] = "(nothing yet)";
	int  rx_len = 0;
	bool last_btn = false;

	lcd.init();
	link.set_blocking(false);
	t.start();

	while (true)
	{
		// send POT1 every 0.5 s
		if (t.elapsed_time() >= 500ms)
		{
			t.reset();
			int len = sprintf(out, "P1=%d\n", pot1.read_u16() >> 4);
			link.write(out, len);
		}

		// send the button state when it changes
		bool btn = joy_centre;
		if (btn != last_btn)
		{
			link.write(btn ? "B1\n" : "B0\n", 3);
			last_btn = btn;
		}

		// collect received bytes into a line, act on '\n'
		char c;
		while (link.read(&c, 1) == 1)
		{
			if (c == '\n')
			{
				rx_line[rx_len] = 0;
				if (rx_line[0] == 'B')		// button message
					led1 = (rx_line[1] == '1');
				else
					strcpy(show, rx_line);	// POT1 message
				rx_len = 0;
			}
			else if (rx_len < 20)
				rx_line[rx_len++] = c;
		}

		lcd.clear();
		lcd.text(0, 0, "TX:");
		lcd.text(24, 0, out);
		lcd.text(0, 12, "RX:");
		lcd.text(24, 12, show);
		lcd.text(0, 24, "p9 TX  p10 RX  9600");
		lcd.update();
		ThisThread::sleep_for(20ms);
	}
}
