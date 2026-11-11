//
// Task 2B : the same cross hair as Task 2A bounces randomly around the
//           edges of the LCD
//
//   seed  : press a keypad key (1~9) to set the seed -> srand(SEED_BASE + key)
//           SEED_BASE = 22010900 (my student ID), so every key gives a
//           different random start position, direction and speed,
//           and the same key always gives the same bouncing path
//           press a key again at any time to restart with a new seed
//
//   start : random centre inside the LCD, random speed 1~3 pixels/frame
//           in x and y with a random sign (+/-)
//   bounce: when the cross hair touches an edge, that direction is reversed
//           and both speeds get a new random value 1~3, so the angle of
//           every bounce is random
//
//   cross hair : circle radius 9 + horizontal and vertical lines 27 pixels long
//                (centre +/- 13), drawn into screen[] then sent with draw_LCD()
//
//   Keypad    : GPA0~5 (quasi mode), ScanKey() returns 1~9, 0 = no key
//   LCD       : 128x64 via SPI3, screen[x + page*128], bit (y & 7), 1 = dark
//   7-segment : number of bounces (0~9999), the 7-segment scan is also the frame delay
//               GPC4~7 select digit (1 = on), GPE0~7 segments (0 = on)
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "NUC1xx.h"
#include "GPIO.h"
#include "SYS.h"
#include "LCD.h"
#include "Seven_Segment.h"
#include "Scankey.h"

#define RADIUS		9		// circle radius
#define ARM		13		// line half length (centre to end of line)
#define SEED_BASE	22010900	// my student ID, added to the key to form the seed
#define FRAME_SCANS	3		// 7-segment scans per frame, 3 x 20 ms = 60 ms (bigger = slower)

#define X_MIN	ARM			// limits of the centre so the whole
#define X_MAX	(LCD_Xmax - 1 - ARM)	// cross hair stays on the LCD
#define Y_MIN	ARM
#define Y_MAX	(LCD_Ymax - 1 - ARM)

unsigned char screen[LCD_Xmax * LCD_Ymax / 8];	// LCD frame buffer, 8 pages x 128 columns

int16_t cx, cy;		// cross hair centre
int16_t dx, dy;		// speed in pixels per frame (sign = direction)
uint16_t bounces = 0;	// number of times an edge was hit

// set one pixel in screen[] (pixels outside the LCD are ignored)
void set_pixel(int16_t x, int16_t y)
{
	if (x < 0 || x >= LCD_Xmax || y < 0 || y >= LCD_Ymax) return;
	screen[x + (y >> 3) * LCD_Xmax] |= 1 << (y & 7);
}

// circle with the midpoint algorithm (8 symmetric points per step)
void draw_circle(int16_t xc, int16_t yc, int16_t r)
{
	int16_t x = r, y = 0;
	int16_t err = 1 - r;

	while (x >= y)
	{
		set_pixel(xc + x, yc + y);	set_pixel(xc - x, yc + y);
		set_pixel(xc + x, yc - y);	set_pixel(xc - x, yc - y);
		set_pixel(xc + y, yc + x);	set_pixel(xc - y, yc + x);
		set_pixel(xc + y, yc - x);	set_pixel(xc - y, yc - x);
		y++;
		if (err < 0)
			err += 2 * y + 1;
		else
		{
			x--;
			err += 2 * (y - x) + 1;
		}
	}
}

// clear the screen, draw the cross hair centred at (x, y) and show it
void draw_crosshair(int16_t x, int16_t y)
{
	int16_t i;

	memset(screen, 0, sizeof(screen));
	for (i = -ARM; i <= ARM; i++)
	{
		set_pixel(x + i, y);		// horizontal line
		set_pixel(x, y + i);		// vertical line
	}
	draw_circle(x, y, RADIUS);
	draw_LCD(screen);
}

// random speed 1~3 pixels per frame
int16_t rand_speed(void)
{
	return 1 + rand() % 3;
}

// display value (0~9999) on the four 7-segments, leading zeros blank
// takes 4 x 5 ms = 20 ms, used as the frame delay
void seg_display(uint16_t value)
{
	int8_t i;
	uint16_t div = 1000;

	for (i = 3; i >= 0; i--)	// i = 3 -> leftmost digit
	{
		close_seven_segment();
		if (value >= div || i == 0)
			show_seven_segment(i, (value / div) % 10);
		DrvSYS_Delay(5000);
		div /= 10;
	}
	close_seven_segment();
}

// seed the random numbers and pick a random start
void start_bounce(uint8_t key)
{
	char TEXT[17];
	int8_t i;

	srand(SEED_BASE + key);

	cx = X_MIN + rand() % (X_MAX - X_MIN + 1);
	cy = Y_MIN + rand() % (Y_MAX - Y_MIN + 1);
	dx = (rand() & 1) ? rand_speed() : -rand_speed();
	dy = (rand() & 1) ? rand_speed() : -rand_speed();
	bounces = 0;

	clear_LCD();			// show the seed for 1 s before starting
	print_Line(0, "Task 2B : Bounce");
	sprintf(TEXT, "Key  : %d        ", key);
	print_Line(1, TEXT);
	sprintf(TEXT, "Seed : %ld", (long)(SEED_BASE + key));
	print_Line(2, TEXT);
	sprintf(TEXT, "X%3d Y%2d        ", cx, cy);
	print_Line(3, TEXT);
	for (i = 0; i < 50; i++)	// 50 x 20 ms = 1 s
		seg_display(key);
}

// move one step and bounce off the edges with a new random speed
void move_crosshair(void)
{
	cx += dx;
	cy += dy;

	if (cx <= X_MIN || cx >= X_MAX)	// left / right edge
	{
		cx = (cx <= X_MIN) ? X_MIN : X_MAX;
		dx = (cx == X_MIN) ? rand_speed() : -rand_speed();
		dy = (dy > 0) ? rand_speed() : -rand_speed();
		bounces++;
	}
	if (cy <= Y_MIN || cy >= Y_MAX)	// top / bottom edge
	{
		cy = (cy <= Y_MIN) ? Y_MIN : Y_MAX;
		dy = (cy == Y_MIN) ? rand_speed() : -rand_speed();
		dx = (dx > 0) ? rand_speed() : -rand_speed();
		bounces++;
	}
	if (bounces > 9999) bounces = 0;
}

int32_t main (void)
{
	uint8_t key;
	uint8_t last_key = 0;
	int8_t  i;

	UNLOCKREG();			// unlock register for programming
	DrvSYS_Open(48000000);		// set System Clock to run at 48MHz
	LOCKREG();			// lock register from programming

	close_seven_segment();
	init_LCD();
	clear_LCD();
	OpenKeyPad();			// initialize 3x3 keypad

	print_Line(0, "Task 2B : Bounce");
	print_Line(1, "Press key 1~9   ");
	print_Line(2, "to set the seed ");
	print_Line(3, "srand(ID + key) ");

	do				// wait for the first key = seed input
		key = ScanKey();
	while (key == 0);
	last_key = key;
	start_bounce(key);

	while (1)
	{
		key = ScanKey();
		if (key != 0 && last_key == 0)	// new key press -> new seed, restart
			start_bounce(key);
		last_key = key;

		move_crosshair();
		draw_crosshair(cx, cy);
		for (i = 0; i < FRAME_SCANS; i++)	// frame delay, 60 ms
			seg_display(bounces);
	}
}
