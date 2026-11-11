//
// Task 2A : draw a cross hair on the LCD and move it with the 3x3 keypad
//           (left, right, up, down or diagonally)
//
//   keypad layout        movement
//     1 2 3              up-left    up      up-right
//     4 5 6              left       centre  right
//     7 8 9              down-left  down    down-right
//
//   hold a key to keep moving, the cross hair stops at the edges of the LCD
//   key 5 puts the cross hair back in the middle of the screen
//
//   cross hair : circle radius 9 + horizontal and vertical lines 27 pixels long
//                (centre +/- 13), drawn into screen[] then sent with draw_LCD()
//
//   Keypad    : GPA0~5 (quasi mode), ScanKey() returns 1~9, 0 = no key
//   LCD       : 128x64 via SPI3, screen[x + page*128], bit (y & 7), 1 = dark
//   7-segment : turned off
//
#include <stdio.h>
#include <string.h>
#include "NUC1xx.h"
#include "GPIO.h"
#include "SYS.h"
#include "LCD.h"
#include "Seven_Segment.h"
#include "Scankey.h"

#define RADIUS	9		// circle radius
#define ARM	13		// line half length (centre to end of line)
#define STEP	2		// pixels moved per frame while a key is held

unsigned char screen[LCD_Xmax * LCD_Ymax / 8];	// LCD frame buffer, 8 pages x 128 columns

// direction of each key (index = key number, 0 = no key)
const int8_t key_dx[10] = {0, -1,  0,  1, -1, 0, 1, -1, 0, 1};
const int8_t key_dy[10] = {0, -1, -1, -1,  0, 0, 0,  1, 1, 1};

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

// clear the screen, draw the cross hair centred at (cx, cy) and show it
void draw_crosshair(int16_t cx, int16_t cy)
{
	int16_t i;

	memset(screen, 0, sizeof(screen));
	for (i = -ARM; i <= ARM; i++)
	{
		set_pixel(cx + i, cy);		// horizontal line
		set_pixel(cx, cy + i);		// vertical line
	}
	draw_circle(cx, cy, RADIUS);
	draw_LCD(screen);
}

// keep the whole cross hair on the LCD
int16_t limit(int16_t v, int16_t lo, int16_t hi)
{
	if (v < lo) return lo;
	if (v > hi) return hi;
	return v;
}

int32_t main (void)
{
	uint8_t key;
	int16_t cx = LCD_Xmax / 2;	// cross hair centre
	int16_t cy = LCD_Ymax / 2;
	int16_t nx, ny;

	UNLOCKREG();			// unlock register for programming
	DrvSYS_Open(48000000);		// set System Clock to run at 48MHz
	LOCKREG();			// lock register from programming

	close_seven_segment();		// 7-segment off
	init_LCD();
	clear_LCD();
	OpenKeyPad();			// initialize 3x3 keypad

	print_Line(0, "Task 2A         ");
	print_Line(1, "Cross hair      ");
	print_Line(2, "keypad 1~9 move ");
	print_Line(3, "5 = centre      ");
	for (nx = 0; nx < 5; nx++)	// 1.5 s title (DrvSYS_Delay max ~349 ms at 48MHz)
		DrvSYS_Delay(300000);

	draw_crosshair(cx, cy);

	while (1)
	{
		key = ScanKey();	// 1~9 while held, 0 = no key

		if (key == 5)		// back to the middle
		{
			nx = LCD_Xmax / 2;
			ny = LCD_Ymax / 2;
		}
		else
		{
			nx = limit(cx + key_dx[key] * STEP, ARM, LCD_Xmax - 1 - ARM);
			ny = limit(cy + key_dy[key] * STEP, ARM, LCD_Ymax - 1 - ARM);
		}

		if (nx != cx || ny != cy)	// redraw only when it moved
		{
			cx = nx;
			cy = ny;
			draw_crosshair(cx, cy);
		}

		DrvSYS_Delay(20000);	// 20 ms per frame
	}
}
