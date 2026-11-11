//
// Assignment 1 - BOARD #1 (TRANSMITTER)
//
//   Reads the two variable resistors POT1 / POT2 on the mbed Application Board
//   and sends the readings to LPC1768 board #2 over UART every 100 ms.
//
//   POT1 : p19 (AnalogIn)      POT2 : p20 (AnalogIn)
//          read_u16() >> 4 = 0 ~ 4095 (12-bit ADC), 8 readings averaged
//
//   UART link to board #2 : 9600 baud, 8N1
//          board #1 p9  (TX) ----> board #2 p10 (RX)
//          board #1 GND  -------- board #2 GND   (common ground is a must)
//
//   Packet (plain text, one line) :  $seq,pot1,pot2,chk\n
//          seq  = 0 ~ 255 packet counter (board #2 uses it to spot lost packets)
//          chk  = (seq + pot1 + pot2) % 256   (board #2 drops bad packets)
//          e.g. "$17,2048,1000,81\n"
//
//   LCD  : "BOARD #1  TX" + both pot values + bars + packet counter
//   LED1 : toggles on every packet sent
//   Serial monitor (USB, 115200, see mbed_app.json) : same packet echoed every 0.5 s
//
//   mbed Application Board pins
//     LCD : MOSI p5, RESET p6, SCK p7, A0 p8, nCS p11
//     p9 / p10 = UART3, also the XBee socket (leave the XBee socket empty)
//
#include "mbed.h"
#include "lcd_c12832.h"

#define SAMPLES		8		// readings averaged per pot
#define SEND_TIME	100ms		// one packet every 100 ms
#define PRINT_EVERY	5		// USB print every 5 packets = 0.5 s
#define LINK_BAUD	9600

AnalogIn     pot1(p19);
AnalogIn     pot2(p20);
DigitalOut   led1(LED1);
BufferedSerial link(p9, p10, LINK_BAUD);	// UART3 : p9 TX, p10 RX

LCD_C12832 lcd;

// average of SAMPLES readings, 0 ~ 4095
uint16_t read_pot(AnalogIn &pot)
{
	uint32_t sum = 0;

	for (int i = 0; i < SAMPLES; i++)
		sum += pot.read_u16() >> 4;
	return sum / SAMPLES;
}

// one pot on the LCD : "P1 2048  50%" + bar on the right
void show_pot(int y, int num, uint16_t value)
{
	char text[24];

	sprintf(text, "P%d %4d %3d%%", num, value, (int)(value * 100 / 4095));
	lcd.text(0, y, text);
	lcd.box(74, y, 54, 7);				// bar frame
	lcd.fill_box(75, y + 1, value * 52 / 4095, 5);	// bar 0 ~ 52 px
}

int main()
{
	uint16_t v1, v2;
	uint8_t  seq = 0;
	uint32_t sent = 0;
	char packet[32];
	char text[24];
	int len;

	Timer pace;			// one packet every SEND_TIME

	lcd.init();
	pace.start();
	printf("\r\nAssignment 1 - BOARD #1 (TX) : POT1/POT2 -> p9 at %d baud\r\n", LINK_BAUD);

	while (true)
	{
		v1 = read_pot(pot1);
		v2 = read_pot(pot2);

		// build and send the packet
		len = sprintf(packet, "$%d,%d,%d,%d\n", seq, v1, v2, (seq + v1 + v2) % 256);
		link.write(packet, len);
		sent++;
		led1 = !led1;

		// LCD
		lcd.clear();
		lcd.text(0, 0, "BOARD #1  TX");
		sprintf(text, "#%lu", (unsigned long)sent);
		lcd.text(128 - strlen(text) * 6, 0, text);
		show_pot(12, 1, v1);
		show_pot(23, 2, v2);
		lcd.update();

		// laptop (optional, board #1 does not need a laptop)
		if (sent % PRINT_EVERY == 0)
			printf("sent %s", packet);

		seq++;					// 255 -> 0 wraps by itself

		// wait with a Timer, not sleep_for : on this board sleep_for woke up
		// only once per second (packets came 1 s apart instead of 100 ms)
		while (pace.elapsed_time() < SEND_TIME) {}
		pace.reset();
	}
}
