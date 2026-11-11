//
// Lab 7 : UART - live webcam on the mbed Application Board LCD
//         (Lab 3 OpenCV camera, now on the mbed LPC1768)
//
//   laptop (lab7_cam_mbed.py, OpenCV face tracking, black/white)
//     -> mbed USB serial (USBTX / USBRX, no USB-TTL cable needed) -> LCD 128x32
//
//   frame from PC : 0xAA 0x55 + 512 bytes (128x32, buffer[x + page*128], bit 1 = dark)
//   reply to PC   : 'K' = frame shown, send the next one
//                   '1'~'6' = joystick (same meaning as the Lab 3 keypad)
//
//   Joystick : down   = '1' full view
//              up     = '2' face zoom
//              left / right = '3' threshold, '4' dither, '5' edges (cycle)
//              centre = '6' invert
//   LED1 : toggles on every frame      LED2 : on while the PC is sending
//   No frame for 2 s -> LCD shows "waiting for PC"
//
//   Serial : 115200 baud 8N1 (BAUD below, must match the Python script)
//            ~20 frames per second at 115200
//
//   mbed Application Board pins
//     LCD      : MOSI p5, RESET p6, SCK p7, A0 p8, nCS p11
//     Joystick : down p12, left p13, centre p14, up p15, right p16 (high = pressed)
//
#include "mbed.h"
#include "lcd_c12832.h"

#define BAUD		115200
#define FRAME_BYTES	512		// 128 x 32 / 8
#define LOST_TIME	2s

BufferedSerial pc(USBTX, USBRX, BAUD);
LCD_C12832 lcd;

DigitalOut led1(LED1);
DigitalOut led2(LED2);

DigitalIn joy_down(p12, PullDown);
DigitalIn joy_left(p13, PullDown);
DigitalIn joy_centre(p14, PullDown);
DigitalIn joy_up(p15, PullDown);
DigitalIn joy_right(p16, PullDown);

uint8_t  frame[FRAME_BYTES];
uint16_t rx_count = 0;
uint8_t  rx_state = 0;		// 0 = wait 0xAA, 1 = wait 0x55, 2 = frame data

// feed one received byte, returns true when a whole frame is in frame[]
bool rx_byte(uint8_t b)
{
	switch (rx_state)
	{
	case 0:
		if (b == 0xAA) rx_state = 1;
		break;
	case 1:
		if (b == 0x55)      { rx_state = 2; rx_count = 0; }
		else if (b != 0xAA) rx_state = 0;
		break;
	default:
		frame[rx_count++] = b;
		if (rx_count == FRAME_BYTES)
		{
			rx_state = 0;
			return true;
		}
	}
	return false;
}

void send_char(char c)
{
	pc.write(&c, 1);
}

void show_waiting()
{
	lcd.clear();
	lcd.text(0, 0,  "Lab7 Cam -> LCD");
	lcd.text(0, 12, "waiting for PC...");
	lcd.text(0, 24, "lab7_cam_mbed.py");
	lcd.update();
}

// joystick, one key per press (edge detected)
void read_joystick()
{
	static int last = 0;
	static int style = 3;			// 3 threshold, 4 dither, 5 edges
	int now = 0;

	if (joy_down)        now = 1;
	else if (joy_up)     now = 2;
	else if (joy_left)   now = 3;
	else if (joy_right)  now = 4;
	else if (joy_centre) now = 5;

	if (now != 0 && last == 0)
	{
		switch (now)
		{
		case 1: send_char('1'); break;
		case 2: send_char('2'); break;
		case 3: style = (style == 3) ? 5 : style - 1; send_char('0' + style); break;
		case 4: style = (style == 5) ? 3 : style + 1; send_char('0' + style); break;
		case 5: send_char('6'); break;
		}
	}
	last = now;
}

int main()
{
	uint8_t chunk[64];
	Timer joy_timer;
	Timer lost_timer;
	bool connected = false;

	lcd.init();
	show_waiting();
	pc.set_blocking(false);

	joy_timer.start();
	lost_timer.start();

	while (true)
	{
		// read whatever has arrived and build the frame
		ssize_t n = pc.read(chunk, sizeof(chunk));
		for (ssize_t i = 0; i < n; i++)
		{
			if (rx_byte(chunk[i]))
			{
				memcpy(lcd.data(), frame, FRAME_BYTES);
				lcd.update();
				send_char('K');		// ready for the next frame
				led1 = !led1;
				led2 = 1;
				connected = true;
				lost_timer.reset();
			}
		}

		// joystick every 20 ms
		if (joy_timer.elapsed_time() >= 20ms)
		{
			joy_timer.reset();
			read_joystick();
		}

		// PC stopped sending
		if (connected && lost_timer.elapsed_time() >= LOST_TIME)
		{
			connected = false;
			led2 = 0;
			rx_state = 0;
			show_waiting();
		}
	}
}
