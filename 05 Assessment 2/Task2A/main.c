//
// Task 2A : draw a cross hair on the LCD and move it with the 3x3 keypad
//           (left, right, up, down or diagonally)
//           extra : the laptop mouse can also move it over UART, and a mouse
//                   click shows the cross hair position (X, Y) on the LCD
//
//   keypad layout        movement
//     1 2 3              up-left    up      up-right
//     4 5 6              left       centre  right
//     7 8 9              down-left  down    down-right
//
//   hold a key to keep moving, the cross hair stops at the edges of the LCD
//   key 5 puts the cross hair back in the middle of the screen
//   the keypad works the same with or without the laptop connected
//
//   mouse (task2a_mouse.py on the laptop, mouse inside its window) :
//     move       -> cross hair goes to the mouse position
//     left click -> for 3 s the LCD corner shows "X:64 Y:32"
//
//   from PC  : 0xAA, x (0~127), y (0~63), flag (1 = click, 0 = move)
//              x and y are never 0xAA, so 0xAA always starts a packet
//   to PC    : "P x y\n" when the cross hair moves, "C x y\n" on a click
//
//   cross hair : circle radius 9 + horizontal and vertical lines 27 pixels long
//                (centre +/- 13), drawn into screen[] then sent with draw_LCD()
//
//   Keypad    : GPA0~5 (quasi mode), ScanKey() returns 1~9, 0 = no key
//   LCD       : 128x64 via SPI3, screen[x + page*128], bit (y & 7), 1 = dark
//   UART1     : 115200 baud, 8N1, CH340 USB-TTL (same wiring as Lab 3)
//               GPB4 = RX1 (JP5 pin 5) <- CH340 TX
//               GPB5 = TX1 (JP5 pin 6) -> CH340 RX
//               GND  =     (JP5 pin 17/18) -- CH340 GND
//   7-segment : turned off
//
#include <stdio.h>
#include <string.h>
#include "NUC1xx.h"
#include "GPIO.h"
#include "SYS.h"
#include "UART.h"
#include "LCD.h"
#include "Font5x7.h"
#include "Seven_Segment.h"
#include "Scankey.h"

#define RADIUS		9		// circle radius
#define ARM		13		// line half length (centre to end of line)
#define STEP		2		// pixels moved per frame while a key is held
#define BAUD_RATE	115200
#define SHOW_FRAMES	150		// click position shown for 150 x 20 ms = 3 s

unsigned char screen[LCD_Xmax * LCD_Ymax / 8];	// LCD frame buffer, 8 pages x 128 columns

// direction of each key (index = key number, 0 = no key)
const int8_t key_dx[10] = {0, -1,  0,  1, -1, 0, 1, -1, 0, 1};
const int8_t key_dy[10] = {0, -1, -1, -1,  0, 0, 0,  1, 1, 1};

// filled by the UART1 interrupt
volatile uint8_t rx_state = 0;		// 0 = wait 0xAA, 1 = x, 2 = y, 3 = flag
volatile uint8_t rx_x, rx_y;
volatile uint8_t mouse_x, mouse_y;	// last mouse position from the PC
volatile uint8_t mouse_new = 0;		// 1 = new mouse position
volatile uint8_t mouse_click = 0;	// 1 = left click

// UART1 interrupt : read every byte in the FIFO and build the 4-byte packet
void UART1_INT_HANDLE(uint32_t u32IntStatus)
{
	uint8_t b;

	while (UART1->FSR.RX_EMPTY == 0)
	{
		b = UART1->DATA;
		if (b == 0xAA)			// start of a packet
		{
			rx_state = 1;
			continue;
		}
		switch (rx_state)
		{
			case 1:	rx_x = b;	rx_state = 2;	break;
			case 2:	rx_y = b;	rx_state = 3;	break;
			case 3:
				mouse_x = rx_x;
				mouse_y = rx_y;
				mouse_new = 1;
				if (b == 1) mouse_click = 1;
				rx_state = 0;
				break;
			default:		// byte outside a packet, ignore
				break;
		}
	}
}

void Init_UART1(void)
{
	STR_UART_T sParam;

	memset(&sParam, 0, sizeof(sParam));
	DrvGPIO_InitFunction(E_FUNC_UART1);		// GPB4 = RX1, GPB5 = TX1

	sParam.u32BaudRate       = BAUD_RATE;
	sParam.u8cDataBits       = DRVUART_DATABITS_8;
	sParam.u8cStopBits       = DRVUART_STOPBITS_1;
	sParam.u8cParity         = DRVUART_PARITY_NONE;
	sParam.u8cRxTriggerLevel = DRVUART_FIFO_1BYTES;
	DrvUART_Open(UART_PORT1, &sParam);

	DrvUART_EnableInt(UART_PORT1, DRVUART_RDAINT, UART1_INT_HANDLE);
}

// send "P x y\n" (move) or "C x y\n" (click) to the PC
void send_pos(char type, int16_t x, int16_t y)
{
	char TEXT[16];

	sprintf(TEXT, "%c %d %d\n", type, x, y);
	DrvUART_Write(UART_PORT1, (uint8_t *)TEXT, strlen(TEXT));
}

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

// write text with the 5x7 font into one page (8 pixel rows) of screen[]
// the bytes are replaced, so the text stays readable over the cross hair
void text_5x7(int16_t x, int8_t page, char *text)
{
	int8_t i;

	while (*text && x <= LCD_Xmax - 6)
	{
		for (i = 0; i < 5; i++)
			screen[x + i + page * LCD_Xmax] = Font5x7[(*text - 0x20) * 5 + i];
		screen[x + 5 + page * LCD_Xmax] = 0;	// 1 pixel gap
		x += 6;
		text++;
	}
}

// clear the screen, draw the cross hair centred at (cx, cy) and show it
// show_xy = 1 -> also write "X:.. Y:.." of (px, py) in the corner away from the cross hair
void draw_crosshair(int16_t cx, int16_t cy, uint8_t show_xy, int16_t px, int16_t py)
{
	char TEXT[12];
	int16_t i;

	memset(screen, 0, sizeof(screen));
	for (i = -ARM; i <= ARM; i++)
	{
		set_pixel(cx + i, cy);		// horizontal line
		set_pixel(cx, cy + i);		// vertical line
	}
	draw_circle(cx, cy, RADIUS);

	if (show_xy)
	{
		sprintf(TEXT, "X:%d Y:%d", px, py);
		text_5x7((cx < LCD_Xmax / 2) ? LCD_Xmax - 6 * strlen(TEXT) : 0,
			 (cy < LCD_Ymax / 2) ? 7 : 0, TEXT);
	}
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
	uint8_t click;
	int16_t cx = LCD_Xmax / 2;	// cross hair centre
	int16_t cy = LCD_Ymax / 2;
	int16_t nx, ny;
	int16_t px = 0, py = 0;		// position of the last click
	uint16_t show = 0;		// frames left to show the click position

	UNLOCKREG();			// unlock register for programming
	DrvSYS_Open(48000000);		// set System Clock to run at 48MHz
	LOCKREG();			// lock register from programming

	close_seven_segment();		// 7-segment off
	init_LCD();
	clear_LCD();
	OpenKeyPad();			// initialize 3x3 keypad
	Init_UART1();

	print_Line(0, "Task 2A         ");
	print_Line(1, "Cross hair      ");
	print_Line(2, "keypad / mouse  ");
	print_Line(3, "5 = centre      ");
	for (nx = 0; nx < 5; nx++)	// 1.5 s title (DrvSYS_Delay max ~349 ms at 48MHz)
		DrvSYS_Delay(300000);

	draw_crosshair(cx, cy, 0, 0, 0);
	send_pos('P', cx, cy);

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

		click = 0;
		if (mouse_new)		// mouse moved or clicked on the laptop
		{
			nx = limit(mouse_x, ARM, LCD_Xmax - 1 - ARM);
			ny = limit(mouse_y, ARM, LCD_Ymax - 1 - ARM);
			mouse_new = 0;
			click = mouse_click;
			mouse_click = 0;
		}

		if (click)		// show this position for 3 s
		{
			px = nx;
			py = ny;
			show = SHOW_FRAMES;
			send_pos('C', px, py);
		}

		if (nx != cx || ny != cy || click || show == 1)	// redraw only when needed
		{
			if (nx != cx || ny != cy) send_pos('P', nx, ny);
			cx = nx;
			cy = ny;
			draw_crosshair(cx, cy, show > 1, px, py);
		}

		if (show > 0) show--;	// count down the 3 s of the click text

		DrvSYS_Delay(20000);	// 20 ms per frame
	}
}
