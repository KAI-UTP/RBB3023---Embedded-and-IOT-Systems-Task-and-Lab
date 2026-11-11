//
// lcd_c12832.h : small driver for the 128x32 LCD (C12832A1Z, ST7565R) on the
//                mbed Application Board, written for Mbed OS 6
//
//   pins   : MOSI p5, RESET p6, SCK p7, A0 p8 (0 = command, 1 = data), nCS p11
//   buffer : 4 pages x 128 columns, buffer[x + (y / 8) * 128], bit (y % 8), 1 = dark
//   use    : init() once, draw into the buffer (clear, pixel, text, box ...),
//            then update()
//
#ifndef LCD_C12832_H
#define LCD_C12832_H

#include "mbed.h"
#include "font5x7.h"

class LCD_C12832 {
public:
	static const int WIDTH = 128;
	static const int HEIGHT = 32;

	LCD_C12832(PinName mosi = p5, PinName sck = p7, PinName reset = p6,
	           PinName a0 = p8, PinName cs = p11)
		: _spi(mosi, NC, sck), _reset(reset), _a0(a0), _cs(cs)
	{
	}

	// call once at the start of main()
	void init()
	{
		_spi.format(8, 3);
		_spi.frequency(20000000);
		_cs = 1;
		_reset = 0;				// hardware reset
		wait_us(50);
		_reset = 1;
		ThisThread::sleep_for(5ms);

		command(0xAE);		// display off
		command(0xA2);		// bias 1/9
		command(0xA0);		// column address normal
		command(0xC8);		// common output reverse
		command(0x22);		// voltage resistor ratio
		command(0x2F);		// power on
		command(0x40);		// start line 0
		command(0xAF);		// display on
		command(0x81);		// contrast ...
		command(0x17);		// ... value
		command(0xA6);		// normal (not inverted)

		clear();
		update();
	}

	void clear() { memset(_buffer, 0, sizeof(_buffer)); }

	void pixel(int x, int y, bool on = true)
	{
		if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
		if (on) _buffer[x + (y / 8) * WIDTH] |=  (1 << (y % 8));
		else    _buffer[x + (y / 8) * WIDTH] &= ~(1 << (y % 8));
	}

	void fill_box(int x, int y, int w, int h, bool on = true)
	{
		for (int j = 0; j < h; j++)
			for (int i = 0; i < w; i++)
				pixel(x + i, y + j, on);
	}

	void box(int x, int y, int w, int h)
	{
		for (int i = 0; i < w; i++) { pixel(x + i, y); pixel(x + i, y + h - 1); }
		for (int j = 0; j < h; j++) { pixel(x, y + j); pixel(x + w - 1, y + j); }
	}

	// one 5x7 character, scale 1 = 6 x 8 pixels, scale 2 = 12 x 16 pixels
	void character(int x, int y, char c, int scale = 1)
	{
		if (c < 0x20 || c > 0x7F) c = ' ';
		for (int col = 0; col < 5; col++)
		{
			uint8_t bits = Font5x7[(c - 0x20) * 5 + col];
			for (int row = 0; row < 8; row++)
				if (bits & (1 << row))
					fill_box(x + col * scale, y + row * scale, scale, scale);
		}
	}

	void text(int x, int y, const char *s, int scale = 1)
	{
		while (*s)
		{
			character(x, y, *s++, scale);
			x += 6 * scale;
		}
	}

	// send the whole buffer to the LCD
	void update()
	{
		for (int page = 0; page < 4; page++)
		{
			command(0x00);			// column 0 (low nibble)
			command(0x10);			// column 0 (high nibble)
			command(0xB0 + page);		// page address
			_a0 = 1;
			_cs = 0;
			for (int i = 0; i < WIDTH; i++)
				_spi.write(_buffer[page * WIDTH + i]);
			_cs = 1;
		}
	}

private:
	void command(uint8_t c)
	{
		_a0 = 0;
		_cs = 0;
		_spi.write(c);
		_cs = 1;
	}

	SPI _spi;
	DigitalOut _reset, _a0, _cs;
	uint8_t _buffer[WIDTH * HEIGHT / 8];
};

#endif
