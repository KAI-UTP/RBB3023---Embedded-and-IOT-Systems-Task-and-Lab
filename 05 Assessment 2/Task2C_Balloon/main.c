//
// Task 2C (extra) : Balloon Shooter game on the LCD
//           balloons float up from the bottom, aim the cross hair and shoot them
//           before they fly away at the top
//
//   menu  : pick how to play with the keypad
//             1 = KEYPAD  aim with the keypad, key 5 shoots
//             2 = MOUSE   aim with the laptop mouse, left click shoots
//             3 = FINGER  laptop webcam follows the index finger, pinch shoots
//           mouse and finger need task2c_control.py running on the laptop
//           the keypad also works in the mouse and finger modes
//
//   keypad layout        movement
//     1 2 3              up-left    up      up-right
//     4 5 6              left       SHOOT   right
//     7 8 9              down-left  down    down-right
//
//   game  : 3 lives (hearts on the LCD + LED5~7), a balloon that escapes at
//           the top costs 1 life, 0 lives = game over
//           every balloon popped = 1 point, more points-> faster balloons
//           and more balloons at the same time
//           game over screen : score, best score, hits / shots and accuracy
//           click / pinch / key 5 to play again, key 4 back to the menu
//   top bar : "NO PC" blinks if the laptop stops sending (mouse / finger mode),
//             "NO HAND" blinks if the webcam cannot see a hand (finger mode)
//   random: srand() seeded with the frame counter when the mode is picked,
//           so every game is different
//
//   LCD frame : balloons -> pop animations -> top bar (score + hearts)
//               -> cross hair inverted (XOR) so it shows on top of balloons
//
//   from PC  : 0xAA, x (0~127), y (0~63), flag (0 = move, 1 = click / pinch, 2 = no hand)
//              the PC sends at least every 0.3 s so the board knows it is there
//   to PC    : "P x y\n" cross hair moved, "S score lives\n" score or lives changed,
//              "M m\n" mode (0 = menu, 1 keypad, 2 mouse, 3 finger), also about once a second
//
//   Keypad    : GPA0~5 (quasi mode), ScanKey() returns 1~9, 0 = no key
//   LCD       : 128x64 via SPI3, screen[x + page*128], bit (y & 7), 1 = dark
//   UART1     : 115200 baud, 8N1, CH340 USB-TTL (same wiring as Task 2A / Lab 3)
//               GPB4 = RX1 (JP5 pin 5) <- CH340 TX
//               GPB5 = TX1 (JP5 pin 6) -> CH340 RX
//               GND  =     (JP5 pin 17/18) -- CH340 GND
//   LEDs      : GPC12~14 = LED5~7 = lives left, output low = on
//   Buzzer    : GPB11, output low = on, short beep = pop, long beep = life lost
//   7-segment : turned off
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "NUC1xx.h"
#include "GPIO.h"
#include "SYS.h"
#include "UART.h"
#include "LCD.h"
#include "Font5x7.h"
#include "Seven_Segment.h"
#include "Scankey.h"

#define RADIUS		6		// cross hair circle radius (smaller than Task 2A to aim)
#define ARM		9		// cross hair line half length
#define STEP		2		// keypad : pixels moved per frame while a key is held
#define BAUD_RATE	115200
#define FRAME_US	15000		// delay per frame (+ LCD drawing time)

#define MAX_BALLOONS	5
#define RX		4		// balloon half width
#define RY		5		// balloon half height
#define HUD_H		9		// top bar : text on page 0 + line at y = 8
#define POP_FRAMES	6		// length of the pop animation
#define LIVES		3

#define ST_MENU		0		// game states
#define ST_PLAY		1
#define ST_OVER		2

#define MODE_KEYPAD	1		// control modes, picked with keypad 1 / 2 / 3 on the menu
#define MODE_MOUSE	2
#define MODE_FINGER	3
#define NO_PC_FRAMES	80		// no packet for ~1.5 s -> "NO PC" on the top bar

unsigned char screen[LCD_Xmax * LCD_Ymax / 8];	// LCD frame buffer, 8 pages x 128 columns
unsigned char cross[LCD_Xmax * LCD_Ymax / 8];	// cross hair pixels, XORed onto screen[]

// direction of each key (index = key number, 0 = no key), key 5 = shoot
const int8_t key_dx[10] = {0, -1,  0,  1, -1, 0, 1, -1, 0, 1};
const int8_t key_dy[10] = {0, -1, -1, -1,  0, 0, 0,  1, 1, 1};

// side to side swing of a rising balloon
const int8_t sway[16] = {0, 1, 1, 2, 2, 2, 1, 1, 0, -1, -1, -2, -2, -2, -1, -1};

// heart for the lives, 5 columns, bit 0 = top row
const unsigned char heart[5] = {0x0C, 0x1E, 0x3C, 0x1E, 0x0C};

typedef struct
{
	uint8_t on;		// 0 = free, 1 = flying, 2 = popping
	int16_t x;		// centre x (without the sway)
	int16_t y16;		// centre y x 16 (sub pixel speed)
	uint8_t speed;		// y16 moved up per frame
	uint8_t phase;		// start point in sway[]
	uint8_t pop;		// frames left of the pop animation
} BALLOON;

BALLOON balloon[MAX_BALLOONS];

int16_t  cx = LCD_Xmax / 2, cy = 36;	// cross hair centre
uint16_t score = 0, best = 0;
uint16_t hits = 0, shots = 0;
uint8_t  lives = LIVES;
uint32_t frame = 0;			// frame counter (also the random seed)
uint8_t  beep = 0;			// frames left of the buzzer beep
uint8_t  flash = 0;			// frames left of the shot flash
uint8_t  spawn_wait = 0;		// frames until the next balloon may appear
uint8_t  mode = MODE_KEYPAD;		// control mode
uint8_t  no_pc = 0;			// 1 = no packet from the PC for NO_PC_FRAMES

// filled by the UART1 interrupt
volatile uint8_t rx_state = 0;		// 0 = wait 0xAA, 1 = x, 2 = y, 3 = flag
volatile uint8_t rx_x, rx_y;
volatile uint8_t mouse_x, mouse_y;	// last mouse / finger position from the PC
volatile uint8_t mouse_new = 0;		// 1 = new position
volatile uint8_t click_count = 0;	// +1 for every click / pinch (main keeps click_done)
volatile uint8_t hand_lost = 0;		// finger mode : 1 = camera does not see a hand
volatile uint8_t rx_packets = 0;	// +1 for every packet (main checks the PC is there)

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
				rx_packets++;
				if (b == 2)		// finger mode : no hand, keep the cross hair
				{
					hand_lost = 1;
					rx_state = 0;
					break;
				}
				hand_lost = 0;
				mouse_x = rx_x;
				mouse_y = rx_y;
				mouse_new = 1;
				if (b == 1) click_count++;
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

// send one text line to the PC
void send_text(char *text)
{
	DrvUART_Write(UART_PORT1, (uint8_t *)text, strlen(text));
}

// LED5~7 (GPC12~14) as lives, buzzer GPB11, all off
void Init_IO(void)
{
	int32_t i;

	for (i = 12; i <= 15; i++)
	{
		DrvGPIO_Open(E_GPC, i, E_IO_OUTPUT);	// GPC12~15 set to output mode
		DrvGPIO_SetBit(E_GPC, i);		// output Hi to turn off LED
	}
	DrvGPIO_Open(E_GPB, 11, E_IO_OUTPUT);	// buzzer
	DrvGPIO_SetBit(E_GPB, 11);		// output Hi = buzzer off
}

// LED5~7 on for each life left
void show_lives(void)
{
	int32_t i;

	for (i = 0; i < LIVES; i++)
	{
		if (i < lives)
			DrvGPIO_ClrBit(E_GPC, 12 + i);
		else
			DrvGPIO_SetBit(E_GPC, 12 + i);
	}
}

// ---------------- drawing into screen[] ----------------

// set one pixel in buf[] (pixels outside the LCD are ignored)
void set_pixel(unsigned char *buf, int16_t x, int16_t y)
{
	if (x < 0 || x >= LCD_Xmax || y < 0 || y >= LCD_Ymax) return;
	buf[x + (y >> 3) * LCD_Xmax] |= 1 << (y & 7);
}

void clr_pixel(unsigned char *buf, int16_t x, int16_t y)
{
	if (x < 0 || x >= LCD_Xmax || y < 0 || y >= LCD_Ymax) return;
	buf[x + (y >> 3) * LCD_Xmax] &= ~(1 << (y & 7));
}

// circle with the midpoint algorithm (8 symmetric points per step)
void draw_circle(unsigned char *buf, int16_t xc, int16_t yc, int16_t r)
{
	int16_t x = r, y = 0;
	int16_t err = 1 - r;

	while (x >= y)
	{
		set_pixel(buf, xc + x, yc + y);	set_pixel(buf, xc - x, yc + y);
		set_pixel(buf, xc + x, yc - y);	set_pixel(buf, xc - x, yc - y);
		set_pixel(buf, xc + y, yc + x);	set_pixel(buf, xc - y, yc + x);
		set_pixel(buf, xc + y, yc - x);	set_pixel(buf, xc - y, yc - x);
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

// text in the middle of a page
void text_centre(int8_t page, char *text)
{
	text_5x7((LCD_Xmax - 6 * (int16_t)strlen(text)) / 2, page, text);
}

// filled balloon (ellipse RX x RY) with a shine, knot and string
void draw_balloon(int16_t x, int16_t y)
{
	int16_t i, j;

	for (j = -RY + 1; j <= RY; j++)		// top row skipped : rounder top
		for (i = -RX; i <= RX; i++)
			if (i * i * RY * RY + j * j * RX * RX <= RX * RX * RY * RY + 20)
				set_pixel(screen, x + i, y + j);
	clr_pixel(screen, x - 2, y - 2);	// shine
	clr_pixel(screen, x - 2, y - 3);
	clr_pixel(screen, x - 1, y - 3);

	set_pixel(screen, x - 1, y + RY + 1);	// knot
	set_pixel(screen, x,     y + RY + 1);
	set_pixel(screen, x + 1, y + RY + 1);
	set_pixel(screen, x,     y + RY + 2);	// string
	set_pixel(screen, x + 1, y + RY + 3);
	set_pixel(screen, x,     y + RY + 4);
	set_pixel(screen, x - 1, y + RY + 5);
}

// pop : 8 short rays moving out from the centre
void draw_pop(int16_t x, int16_t y, uint8_t left)
{
	int16_t r = 2 + (POP_FRAMES - left);
	int8_t dx, dy;

	for (dx = -1; dx <= 1; dx++)
		for (dy = -1; dy <= 1; dy++)
			if (dx || dy)
			{
				set_pixel(screen, x + dx * r, y + dy * r);
				set_pixel(screen, x + dx * (r + 1), y + dy * (r + 1));
			}
}

// top bar : score on the left, hearts on the right, line under it
void draw_hud(void)
{
	char TEXT[17];
	int16_t i, j;

	memset(screen, 0, LCD_Xmax);		// clear page 0
	sprintf(TEXT, "SCORE %d", score);
	text_5x7(0, 0, TEXT);
	for (i = 0; i < lives; i++)
		for (j = 0; j < 5; j++)
			screen[LCD_Xmax - 6 * (i + 1) + j] = heart[j];
	if (frame & 16)				// blinking warning in the middle
	{
		if (mode != MODE_KEYPAD && no_pc)
			text_5x7(56, 0, "NO PC");
		else if (mode == MODE_FINGER && hand_lost)
			text_5x7(56, 0, "NO HAND");
	}
	for (i = 0; i < LCD_Xmax; i++)
		set_pixel(screen, i, HUD_H - 1);
}

// cross hair inverted on top of everything (XOR), so it shows over the balloons
void draw_crosshair(void)
{
	int16_t i;

	memset(cross, 0, sizeof(cross));
	for (i = -ARM; i <= ARM; i++)
	{
		set_pixel(cross, cx + i, cy);		// horizontal line
		set_pixel(cross, cx, cy + i);		// vertical line
	}
	draw_circle(cross, cx, cy, RADIUS);
	if (flash)					// shot fired : small ring
		draw_circle(cross, cx, cy, 2);
	for (i = 0; i < sizeof(screen); i++)
		screen[i] ^= cross[i];
}

// x of a balloon now (with the swing)
int16_t balloon_x(BALLOON *b)
{
	return b->x + sway[((frame >> 2) + b->phase) & 15];
}

// ---------------- game ----------------

void new_game(void)
{
	uint8_t i;
	char TEXT[24];

	srand(frame + cx * 131 + cy);	// frame counter = time waited on the menu
	for (i = 0; i < MAX_BALLOONS; i++)
		balloon[i].on = 0;
	score = hits = shots = 0;
	lives = LIVES;
	spawn_wait = 0;
	show_lives();
	sprintf(TEXT, "S %d %d\n", score, lives);
	send_text(TEXT);
}

// harder with more points : more balloons, faster, less wait between them
void spawn_balloon(void)
{
	uint8_t i, flying = 0;
	uint8_t max_on = 2 + score / 8;
	int16_t wait;

	for (i = 0; i < MAX_BALLOONS; i++)
		if (balloon[i].on == 1) flying++;
	if (max_on > MAX_BALLOONS) max_on = MAX_BALLOONS;
	if (spawn_wait > 0) { spawn_wait--; return; }
	if (flying >= max_on) return;

	for (i = 0; i < MAX_BALLOONS; i++)
		if (balloon[i].on == 0)
		{
			balloon[i].on    = 1;
			balloon[i].x     = RX + 3 + rand() % (LCD_Xmax - 2 * (RX + 3));
			balloon[i].y16   = (LCD_Ymax + RY) * 16;		// just under the screen
			balloon[i].speed = 4 + rand() % 6 + (score / 4 < 12 ? score / 4 : 12);
			balloon[i].phase = rand() % 16;
			wait = 20 + rand() % 30 - score / 2;	// 20~49 frames, shorter with more points
			spawn_wait = (wait < 8) ? 8 : wait;
			return;
		}
}

// move the balloons up, count the ones that escaped
void move_balloons(void)
{
	uint8_t i;
	char TEXT[24];

	for (i = 0; i < MAX_BALLOONS; i++)
	{
		if (balloon[i].on == 1)
		{
			balloon[i].y16 -= balloon[i].speed;
			if ((balloon[i].y16 >> 4) + RY + 5 < HUD_H)	// string gone into the top bar
			{
				balloon[i].on = 0;
				if (lives > 0) lives--;
				beep = 10;				// long beep
				show_lives();
				sprintf(TEXT, "S %d %d\n", score, lives);
				send_text(TEXT);
			}
		}
		else if (balloon[i].on == 2 && --balloon[i].pop == 0)
			balloon[i].on = 0;
	}
}

// shoot at the cross hair centre, pop the first balloon that is hit
void shoot(void)
{
	uint8_t i;
	int16_t dx, dy;
	char TEXT[24];

	shots++;
	flash = 2;
	for (i = 0; i < MAX_BALLOONS; i++)
	{
		if (balloon[i].on != 1) continue;
		dx = cx - balloon_x(&balloon[i]);
		dy = cy - (balloon[i].y16 >> 4);
		if (dx * dx * RY * RY + dy * dy * RX * RX <= (RX + 1) * (RX + 1) * (RY + 1) * (RY + 1))
		{
			balloon[i].on  = 2;
			balloon[i].pop = POP_FRAMES;
			balloon[i].x   = balloon_x(&balloon[i]);	// pop where it is now
			balloon[i].phase = 0;
			score++;
			hits++;
			if (score > best) best = score;
			beep = 3;				// short beep
			sprintf(TEXT, "S %d %d\n", score, lives);
			send_text(TEXT);
			return;
		}
	}
}

void draw_game(void)
{
	uint8_t i;
	int16_t y;

	memset(screen, 0, sizeof(screen));
	for (i = 0; i < MAX_BALLOONS; i++)
	{
		y = balloon[i].y16 >> 4;
		if (balloon[i].on == 1)
			draw_balloon(balloon_x(&balloon[i]), y);
		else if (balloon[i].on == 2)
			draw_pop(balloon[i].x, y, balloon[i].pop);
	}
	draw_hud();
	draw_crosshair();
	draw_LCD(screen);
}

// start menu : pick how to play with keypad 1 / 2 / 3
void draw_menu(void)
{
	char TEXT[17];

	memset(screen, 0, sizeof(screen));
	draw_balloon(12, 28 + sway[(frame >> 2) & 15]);		// two balloons
	draw_balloon(115, 28 + sway[((frame >> 2) + 8) & 15]);	// bobbing up and down
	text_centre(0, "BALLOON SHOOTER");
	text_centre(2, "1  KEYPAD");
	text_centre(3, "2  MOUSE ");
	text_centre(4, "3  FINGER");
	text_centre(6, "PRESS 1, 2 OR 3");
	sprintf(TEXT, "BEST %d", best);
	text_centre(7, TEXT);
	draw_LCD(screen);
}

void draw_over(void)
{
	char TEXT[22];

	memset(screen, 0, sizeof(screen));
	text_centre(1, "GAME OVER");
	sprintf(TEXT, "SCORE %d  BEST %d", score, best);
	text_centre(3, TEXT);
	sprintf(TEXT, "HIT %d/%d  %d%%", hits, shots, shots ? hits * 100 / shots : 0);
	text_centre(4, TEXT);
	text_centre(6, (mode == MODE_KEYPAD) ? "KEY 5 = AGAIN" :
		       (mode == MODE_MOUSE)  ? "CLICK/5 = AGAIN" : "PINCH/5 = AGAIN");
	text_centre(7, "KEY 4 = MENU");
	draw_crosshair();
	draw_LCD(screen);
}

// keep the cross hair centre on the LCD
int16_t limit(int16_t v, int16_t lo, int16_t hi)
{
	if (v < lo) return lo;
	if (v > hi) return hi;
	return v;
}

int32_t main (void)
{
	uint8_t key, last_key = 0, new_key;
	uint8_t fire;
	uint8_t click_done = 0;		// clicks already used
	uint8_t state = ST_MENU;
	uint8_t wait = 0;		// frames before game over accepts START
	uint8_t last_packets = 0;	// rx_packets seen last frame
	uint8_t quiet = 0;		// frames without a packet from the PC
	int16_t nx, ny;
	char TEXT[24];

	UNLOCKREG();			// unlock register for programming
	DrvSYS_Open(48000000);		// set System Clock to run at 48MHz
	LOCKREG();			// lock register from programming

	close_seven_segment();		// 7-segment off
	Init_IO();			// LEDs and buzzer off
	init_LCD();
	clear_LCD();
	OpenKeyPad();			// initialize 3x3 keypad
	Init_UART1();

	while (1)
	{
		frame++;

		// ---- input : keypad always, mouse / finger from the PC in those modes ----
		key = ScanKey();	// 1~9 while held, 0 = no key
		new_key = (key != last_key) ? key : 0;	// key just pressed (0 = none)
		last_key = key;
		nx = limit(cx + key_dx[key] * STEP, 0, LCD_Xmax - 1);
		ny = limit(cy + key_dy[key] * STEP, 0, LCD_Ymax - 1);
		fire = (new_key == 5);	// key 5 once per press

		if (mouse_new)
		{
			if (mode != MODE_KEYPAD && state != ST_MENU)
			{
				nx = limit(mouse_x, 0, LCD_Xmax - 1);
				ny = limit(mouse_y, 0, LCD_Ymax - 1);
			}
			mouse_new = 0;
		}
		if (click_count != click_done)	// one or more clicks / pinches
		{
			click_done = click_count;
			if (mode != MODE_KEYPAD && state != ST_MENU) fire = 1;
		}

		if (rx_packets != last_packets)	// is the PC still sending ?
		{
			last_packets = rx_packets;
			quiet = 0;
		}
		else if (quiet < NO_PC_FRAMES)
			quiet++;
		no_pc = (quiet >= NO_PC_FRAMES);

		if ((frame & 63) == 0)		// tell the PC the mode about once a second
		{
			sprintf(TEXT, "M %d\n", (state == ST_MENU) ? 0 : mode);
			send_text(TEXT);
		}
		if (nx != cx || ny != cy)
		{
			cx = nx;
			cy = ny;
			sprintf(TEXT, "P %d %d\n", cx, cy);
			send_text(TEXT);
		}

		// ---- game states ----
		switch (state)
		{
			case ST_MENU:
				if (new_key >= MODE_KEYPAD && new_key <= MODE_FINGER)
				{
					mode = new_key;
					sprintf(TEXT, "M %d\n", mode);
					send_text(TEXT);
					cx = LCD_Xmax / 2;
					cy = 36;
					new_game();
					state = ST_PLAY;
				}
				else
					draw_menu();
				break;

			case ST_PLAY:
				if (fire) shoot();
				spawn_balloon();
				move_balloons();
				draw_game();
				if (lives == 0)
				{
					state = ST_OVER;
					wait = 40;	// 40 frames before a click restarts
					beep = 25;
				}
				break;

			default:		// ST_OVER
				if (wait > 0) wait--;
				else if (new_key == 4)	// back to the menu
				{
					state = ST_MENU;
					sprintf(TEXT, "M 0\n");
					send_text(TEXT);
					break;
				}
				else if (fire)
				{
					new_game();
					state = ST_PLAY;
					break;
				}
				draw_over();
				break;
		}

		// ---- buzzer and shot flash ----
		if (beep > 0)
		{
			DrvGPIO_ClrBit(E_GPB, 11);	// buzzer on
			beep--;
		}
		else
			DrvGPIO_SetBit(E_GPB, 11);	// buzzer off
		if (flash > 0) flash--;

		DrvSYS_Delay(FRAME_US);
	}
}
