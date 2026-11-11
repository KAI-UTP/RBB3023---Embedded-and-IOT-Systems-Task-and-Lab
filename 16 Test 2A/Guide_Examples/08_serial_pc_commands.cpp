//
// Example 08 : BufferedSerial - talk to the PC (Tera Term / Mbed Studio serial monitor)
//
//   USB serial (USBTX / USBRX) at 115200 baud
//   Type on the PC:  1 ~ 4 = toggle LED1 ~ LED4,  s = status,  anything else = echoed
//   The board also sends "tick n" every 2 s (Timer, no blocking)
//
//   Lecture (mbed 2): Serial pc(USBTX, USBRX); pc.printf(...); pc.getc(); pc.readable()
//   Mbed OS 6 has no Serial class -> BufferedSerial read() / write()
//   Do not mix printf and a BufferedSerial on the same USBTX/USBRX: use one of them.
//
#include "mbed.h"

BufferedSerial pc(USBTX, USBRX, 115200);
DigitalOut leds[4] = { DigitalOut(LED1), DigitalOut(LED2),
                       DigitalOut(LED3), DigitalOut(LED4) };
Timer t;

// send a C string
void send(const char *s)
{
	pc.write(s, strlen(s));
}

int main()
{
	char line[48];
	int  ticks = 0;

	pc.set_blocking(false);		// read() returns at once if nothing came
	t.start();
	send("\r\nType 1~4 to toggle LEDs, s = status\r\n");

	while (true)
	{
		char c;

		if (pc.read(&c, 1) == 1)	// one byte received
		{
			if (c >= '1' && c <= '4')
			{
				int n = c - '1';
				leds[n] = !leds[n];
				sprintf(line, "LED%d %s\r\n", n + 1, leds[n] ? "ON" : "OFF");
				send(line);
			}
			else if (c == 's')
			{
				sprintf(line, "LEDs %d%d%d%d\r\n",
				        leds[0].read(), leds[1].read(), leds[2].read(), leds[3].read());
				send(line);
			}
			else
			{
				sprintf(line, "you typed '%c' (0x%02X)\r\n", c, c);
				send(line);
			}
		}

		if (t.elapsed_time() >= 2s)
		{
			t.reset();
			sprintf(line, "tick %d\r\n", ++ticks);
			send(line);
		}
	}
}
