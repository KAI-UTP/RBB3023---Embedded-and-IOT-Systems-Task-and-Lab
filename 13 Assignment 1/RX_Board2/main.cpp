//
// Assignment 1 - BOARD #2 (RECEIVER)
//
//   Receives the POT1 / POT2 readings sent by LPC1768 board #1 over UART and
//   shows them on
//     1. the LCD of this board (#2)
//     2. the laptop connected to this board (USB serial, 115200 baud)
//
//   UART link from board #1 : 9600 baud, 8N1
//          board #1 p9  (TX) ----> board #2 p10 (RX)
//          board #1 GND  -------- board #2 GND   (common ground is a must)
//
//   Packet (plain text, one line) :  $seq,pot1,pot2,chk\n
//          chk = (seq + pot1 + pot2) % 256, bad packets are counted and dropped
//          a jump in seq = packets lost on the way
//
//   LCD  : "BOARD #2  RX" + packet counter, POT1 / POT2 value, % and bar
//          "NO SIGNAL" when nothing arrives for 1 s (wire off / board #1 off),
//          with the raw byte count and bad packets (bytes 0 = check the wire)
//   LED1 : toggles on every good packet      LED4 : on = no signal
//   Laptop : one line per good packet, e.g.
//          (NO SIGNAL lines every 1 s while nothing good arrives)
//          RX #123  POT1 = 2048 ( 50%) 1.65V   POT2 = 1000 ( 24%) 0.80V  lost 0 bad 0
//          (view with Mbed Studio serial monitor / Tera Term / asg1_viewer.py)
//
//   mbed Application Board pins
//     LCD : MOSI p5, RESET p6, SCK p7, A0 p8, nCS p11
//     p9 / p10 = UART3, also the XBee socket (leave the XBee socket empty)
//
#include "mbed.h"
#include "lcd_c12832.h"

#define LINK_BAUD	9600
#define TIMEOUT		1s		// no packet for 1 s = NO SIGNAL
#define LCD_TIME	100ms		// LCD refresh

DigitalOut     led1(LED1);
DigitalOut     led4(LED4);
BufferedSerial link(p9, p10, LINK_BAUD);	// UART3 : p9 TX, p10 RX

LCD_C12832 lcd;

uint16_t pot1 = 0, pot2 = 0;		// last good readings
uint32_t good = 0, bad = 0, lost = 0;
uint32_t bytes = 0;			// raw bytes seen on p10 (to check the wire)
int      last_seq = -1;

// one pot on the LCD : "P1 2048  50%" + bar on the right
void show_pot(int y, int num, uint16_t value)
{
	char text[24];

	sprintf(text, "P%d %4d %3d%%", num, value, (int)(value * 100 / 4095));
	lcd.text(0, y, text);
	lcd.box(74, y, 54, 7);				// bar frame
	lcd.fill_box(75, y + 1, value * 52 / 4095, 5);	// bar 0 ~ 52 px
}

// check one received line "$seq,pot1,pot2,chk" (without '\n'), true if good
bool parse_packet(const char *line)
{
	int seq, v1, v2, chk;

	if (sscanf(line, "$%d,%d,%d,%d", &seq, &v1, &v2, &chk) != 4)
		return false;
	if (seq < 0 || seq > 255 || v1 < 0 || v1 > 4095 || v2 < 0 || v2 > 4095)
		return false;
	if ((seq + v1 + v2) % 256 != chk)
		return false;

	// gap in seq = lost packets ; seq 0 after anything but 255 = board #1
	// was reset (counter restarted), not lost packets
	if (last_seq >= 0 && !(seq == 0 && last_seq != 255))
		lost += (seq - last_seq - 1 + 256) % 256;
	last_seq = seq;
	pot1 = v1;
	pot2 = v2;
	return true;
}

// send the readings to the laptop
void print_laptop()
{
	uint32_t mv1 = pot1 * 3300 / 4095, mv2 = pot2 * 3300 / 4095;

	printf("RX #%lu  POT1 = %4d (%3d%%) %d.%02dV   POT2 = %4d (%3d%%) %d.%02dV  lost %lu bad %lu\r\n",
	       (unsigned long)good,
	       pot1, (int)(pot1 * 100 / 4095), (int)(mv1 / 1000), (int)(mv1 % 1000 / 10),
	       pot2, (int)(pot2 * 100 / 4095), (int)(mv2 / 1000), (int)(mv2 % 1000 / 10),
	       (unsigned long)lost, (unsigned long)bad);
}

void lcd_display(bool signal)
{
	char text[24];

	lcd.clear();
	lcd.text(0, 0, "BOARD #2  RX");
	sprintf(text, "#%lu", (unsigned long)good);
	lcd.text(128 - strlen(text) * 6, 0, text);

	if (signal)
	{
		show_pot(12, 1, pot1);
		show_pot(23, 2, pot2);
	}
	else
	{
		lcd.text(10, 10, "NO SIGNAL", 2);
		sprintf(text, "bytes %lu bad %lu", (unsigned long)bytes, (unsigned long)bad);
		lcd.text(0, 25, text);
	}
	lcd.update();
}

int main()
{
	char line[32];
	int  pos = 0;
	char c;
	bool signal = false;
	Timer since_packet;		// time since the last good packet
	Timer since_lcd;
	Timer since_status;		// NO SIGNAL message to the laptop every 1 s

	link.set_blocking(false);	// read() returns -EAGAIN when empty
	lcd.init();
	printf("\r\nAssignment 1 - BOARD #2 (RX) : p10 at %d baud -> LCD + laptop\r\n", LINK_BAUD);

	since_packet.start();
	since_lcd.start();
	since_status.start();
	lcd_display(false);

	while (true)
	{
		// collect bytes into a line, handle it at '\n'
		while (link.read(&c, 1) == 1)
		{
			bytes++;
			if (c == '$')			// start of packet, resync
				pos = 0;
			if (c == '\n')
			{
				line[pos] = '\0';
				pos = 0;
				if (parse_packet(line))
				{
					good++;
					led1 = !led1;
					since_packet.reset();
					print_laptop();
				}
				else
				{
					bad++;
				}
			}
			else if (c != '\r' && pos < (int)sizeof(line) - 1)
			{
				line[pos++] = c;
			}
		}

		signal = (good > 0) && (since_packet.elapsed_time() < TIMEOUT);
		led4 = !signal;

		if (!signal && since_status.elapsed_time() >= 1s)
		{
			since_status.reset();
			printf("NO SIGNAL  bytes %lu  bad %lu\r\n", (unsigned long)bytes, (unsigned long)bad);
		}

		if (since_lcd.elapsed_time() >= LCD_TIME)
		{
			since_lcd.reset();
			lcd_display(signal);
		}

		ThisThread::sleep_for(5ms);	// 9600 baud ~ 1 byte/ms, rx buffer 256 bytes
	}
}
