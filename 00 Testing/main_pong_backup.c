//
// Two-Board Pong : one Pong table stretched over the LCDs of TWO Nu-LB-NUC140
//                  boards, the ball flies from one board's screen to the other
//
//   both boards run this SAME program, after reset pick the side :
//     key 1 = LEFT  board (runs the game : ball, scores, sends the picture data)
//     key 3 = RIGHT board (sends its paddle, shows what the LEFT board says)
//
//   play  : key 2 = paddle up, key 8 = paddle down (hold), key 5 = serve
//           the ball goes faster after every hit, where it hits the paddle
//           sets the angle (edge of the paddle = steep)
//           miss the ball -> the other side gets 1 point, first to 5 wins
//           after the game key 5 on either board plays again
//
//   table : 256 x 64 pixels, x 0~127 on the LEFT LCD, x 128~255 on the RIGHT LCD
//           top row (page 0) = score bar "LEFT 3 : 2 RIGHT"
//
//   link  : UART1 <-> UART1, 115200 baud, 8N1, cross wired (TX to RX)
//     LEFT  GPB5 = TX1 (JP5 pin 6)  ->  RIGHT GPB4 = RX1 (JP5 pin 5)
//     LEFT  GPB4 = RX1 (JP5 pin 5)  <-  RIGHT GPB5 = TX1 (JP5 pin 6)
//     LEFT  GND (JP5 pin 17/18)     --  RIGHT GND (JP5 pin 17/18)
//   packet (12 bytes, every frame) : 0xA5 0x5A type d0~d7 checksum
//     type 'M' LEFT -> RIGHT : ball x, ball y, left pad, right pad,
//                              left score, right score, state | scorer << 4, 0 (spare)
//     type 'S' RIGHT -> LEFT : right pad, key 5 press counter, 0 ...
//     checksum = type + d0 + ... + d7 (8 bit), bad packets are dropped
//   no packet for 50 frames (~1.5 s) -> "WAITING FOR THE OTHER BOARD"
//   both boards on the same side -> warning on the LCD, reset one board
//
//   Keypad    : GPA0~5 (quasi mode), ScanKey() returns 1~9, 0 = no key
//   LCD       : 128x64 via SPI3, screen[x + page*128], bit (y & 7), 1 = dark
//   7-segment : score, leftmost digit = LEFT, rightmost digit = RIGHT
//               the 2 x 10 ms scan is also the frame delay
//   Buzzer    : GPB11 kept off (no sound)
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

#define BAUD_RATE	115200
#define WORLD_W		256		// table width over both LCDs
#define TOP		9		// play area starts under the score bar
#define PAD_H		14		// paddle height
#define PAD_XL		2		// left paddle columns 2~3
#define PAD_XR		252		// right paddle columns 252~253
#define PAD_STEP	3		// paddle pixels per frame while the key is held
#define BALL		4		// ball is a solid 4 x 4 block (easy to see)
#define V_START		26		// ball x speed at serve (1/16 pixel per frame)
#define V_MAX		48		// fastest x speed
#define WIN_SCORE	5
#define LINK_LOST	50		// frames without a packet -> link lost
#define PKT_LEN		8		// data bytes in a packet

#define SIDE_NONE	0		// not picked yet
#define SIDE_LEFT	1
#define SIDE_RIGHT	2

#define G_WAIT		0		// game states (sent to the RIGHT board)
#define G_READY		1		// press 5 to serve
#define G_PLAY		2
#define G_POINT		3		// short pause after a point
#define G_OVER		4


unsigned char screen[LCD_Xmax * LCD_Ymax / 8];	// LCD frame buffer, 8 pages x 128 columns

uint8_t  side = SIDE_NONE;
uint8_t  gstate = G_WAIT;
uint8_t  scorer = 0;		// who got the last point (SIDE_LEFT / SIDE_RIGHT)
int16_t  bx, by;		// ball top-left on the table (pixels)
int16_t  pad_l, pad_r;		// paddle tops
uint8_t  score_l = 0, score_r = 0;
uint8_t  conflict = 0;		// 1 = other board picked the same side
uint32_t frame = 0;

// LEFT board only : ball physics
int16_t  bx16, by16;		// ball position x 16 (sub pixel)
int16_t  vx, vy;		// ball speed (1/16 pixel per frame)
int8_t   serve_dir = 1;		// +1 serve to the right, -1 to the left
uint8_t  timer = 0;		// frames left of the G_POINT pause
uint8_t  seeded = 0;

// filled by the UART1 interrupt
volatile uint8_t rx_state = 0;	// 0 = wait 0xA5, 1 = wait 0x5A, 2 = type, 3 = data, 4 = checksum
volatile uint8_t rx_i, rx_type, rx_sum;
volatile uint8_t rx_buf[PKT_LEN];
volatile uint8_t pkt[PKT_LEN];	// last good packet
volatile uint8_t pkt_type;
volatile uint8_t pkt_ready = 0;	// 1 = pkt[] full, main loop has not taken it yet

// UART1 interrupt : read every byte in the FIFO and build the packet
void UART1_INT_HANDLE(uint32_t u32IntStatus)
{
	uint8_t b, i;

	while (UART1->FSR.RX_EMPTY == 0)
	{
		b = UART1->DATA;
		switch (rx_state)
		{
			case 0:				// wait for sync byte 1
				if (b == 0xA5) rx_state = 1;
				break;
			case 1:				// wait for sync byte 2
				if (b == 0x5A) rx_state = 2;
				else if (b != 0xA5) rx_state = 0;
				break;
			case 2:				// packet type
				rx_type = b;
				rx_sum = b;
				rx_i = 0;
				rx_state = 3;
				break;
			case 3:				// data bytes
				rx_buf[rx_i++] = b;
				rx_sum += b;
				if (rx_i >= PKT_LEN) rx_state = 4;
				break;
			default:			// checksum : keep the packet only if it is right
				if (b == rx_sum && !pkt_ready)
				{
					for (i = 0; i < PKT_LEN; i++)
						pkt[i] = rx_buf[i];
					pkt_type = rx_type;
					pkt_ready = 1;
				}
				rx_state = 0;
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

// 0xA5 0x5A type d0~d7 checksum
void send_packet(uint8_t type, uint8_t *d)
{
	uint8_t buf[PKT_LEN + 4];
	uint8_t i, sum = type;

	buf[0] = 0xA5;
	buf[1] = 0x5A;
	buf[2] = type;
	for (i = 0; i < PKT_LEN; i++)
	{
		buf[3 + i] = d[i];
		sum += d[i];
	}
	buf[3 + PKT_LEN] = sum;
	DrvUART_Write(UART_PORT1, buf, PKT_LEN + 4);
}

// ---------------- drawing into screen[] ----------------

// set one pixel in screen[] (pixels outside the LCD are ignored)
void set_pixel(int16_t x, int16_t y)
{
	if (x < 0 || x >= LCD_Xmax || y < 0 || y >= LCD_Ymax) return;
	screen[x + (y >> 3) * LCD_Xmax] |= 1 << (y & 7);
}

// filled box given in TABLE x, drawn only if it is on this board's half
void table_box(int16_t x, int16_t y, int16_t w, int16_t h)
{
	int16_t i, j;
	int16_t off = (side == SIDE_RIGHT) ? LCD_Xmax : 0;

	for (i = 0; i < w; i++)
		for (j = 0; j < h; j++)
			set_pixel(x + i - off, y + j);
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

// two lines of message in the middle of the play area
void message(char *line1, char *line2)
{
	text_centre(3, line1);
	text_centre(4, line2);
}

void draw_menu(void)
{
	memset(screen, 0, sizeof(screen));
	text_centre(0, "TWO-BOARD PONG");
	text_centre(2, "1 = LEFT  BOARD");
	text_centre(3, "3 = RIGHT BOARD");
	text_centre(5, "2 UP    8 DOWN");
	text_centre(6, "5 = SERVE");
	text_centre(7, "PICK A SIDE");
	draw_LCD(screen);
}

void draw_table(void)
{
	char TEXT[22];
	int16_t i, y;
	uint8_t me = side;

	memset(screen, 0, sizeof(screen));

	// score bar
	sprintf(TEXT, "LEFT %d : %d RIGHT", score_l, score_r);
	text_centre(0, TEXT);
	for (i = 0; i < LCD_Xmax; i++)
		set_pixel(i, TOP - 1);

	// dashed net on the edge where the two boards meet
	for (y = TOP + 1; y < LCD_Ymax; y += 4)
	{
		set_pixel((side == SIDE_LEFT) ? LCD_Xmax - 1 : 0, y);
		set_pixel((side == SIDE_LEFT) ? LCD_Xmax - 1 : 0, y + 1);
	}

	// paddles and ball (table_box only draws the part on this board)
	table_box(PAD_XL, pad_l, 2, PAD_H);
	table_box(PAD_XR, pad_r, 2, PAD_H);
	if (gstate == G_PLAY || gstate == G_READY)
		table_box(bx, by, BALL, BALL);

	// messages
	if (conflict)
		message("BOTH BOARDS SAME", "SIDE! RESET ONE");
	else if (gstate == G_WAIT)
		message("WAITING FOR THE", "OTHER BOARD...");
	else if (gstate == G_READY)
		message("PRESS 5", "TO SERVE");
	else if (gstate == G_POINT)
		message((scorer == SIDE_LEFT) ? "LEFT SCORES!" : "RIGHT SCORES!", "");
	else if (gstate == G_OVER)
	{
		if ((score_l > score_r) == (me == SIDE_LEFT))
			message("YOU WIN !", "5 = PLAY AGAIN");
		else
			message("YOU LOSE", "5 = PLAY AGAIN");
	}
	draw_LCD(screen);
}

// score on the 7-segments : leftmost = LEFT, rightmost = RIGHT, 2 x 10 ms
void seg_score(void)
{
	close_seven_segment();
	show_seven_segment(3, score_l % 10);
	DrvSYS_Delay(10000);
	close_seven_segment();
	show_seven_segment(0, score_r % 10);
	DrvSYS_Delay(10000);
	close_seven_segment();
}

int16_t limit(int16_t v, int16_t lo, int16_t hi)
{
	if (v < lo) return lo;
	if (v > hi) return hi;
	return v;
}

// ---------------- game (LEFT board only) ----------------

// ball in the middle of the table, waiting for the serve
void centre_ball(void)
{
	bx16 = (WORLD_W / 2 - BALL / 2) * 16;
	by16 = ((TOP + LCD_Ymax) / 2 - BALL / 2) * 16;
}

void serve(void)
{
	centre_ball();
	vx = serve_dir * V_START;
	vy = rand() % 21 - 10;			// -10 ~ +10
	if (vy > -4 && vy < 4) vy = (rand() & 1) ? 6 : -6;
	gstate = G_PLAY;
}

// paddle hit : faster, angle from where the ball hit the paddle
void bounce(int16_t pad)
{
	vy = ((by + BALL / 2) - (pad + PAD_H / 2)) * 3;
	vy = limit(vy, -26, 26);
}

void point_to(uint8_t who)
{
	if (who == SIDE_LEFT) score_l++;
	else score_r++;
	scorer = who;
	serve_dir = (who == SIDE_LEFT) ? 1 : -1;	// serve to the one who missed
	gstate = G_POINT;
	timer = 40;					// ~1.2 s pause
}

void game_step(uint8_t link, uint8_t start)
{
	if (!link)
	{
		gstate = G_WAIT;
		return;
	}

	switch (gstate)
	{
		case G_WAIT:				// link back : wait for a serve
			centre_ball();
			gstate = G_READY;
			break;

		case G_READY:
			if (start)
			{
				if (!seeded)		// random seed = time until the first serve
				{
					srand(frame);
					seeded = 1;
				}
				serve();
			}
			break;

		case G_PLAY:
			bx16 += vx;
			by16 += vy;
			if ((by16 >> 4) < TOP)				// top wall
			{
				by16 = TOP * 16;
				vy = -vy;
			}
			if ((by16 >> 4) > LCD_Ymax - BALL)		// bottom wall
			{
				by16 = (LCD_Ymax - BALL) * 16;
				vy = -vy;
			}
			bx = bx16 >> 4;
			by = by16 >> 4;

			// left paddle (window 5 pixels wide, wider than the fastest step)
			if (vx < 0 && bx <= PAD_XL + 2 && bx > PAD_XL - 3
			    && by + BALL > pad_l && by < pad_l + PAD_H)
			{
				bx16 = (PAD_XL + 2) * 16;
				vx = -vx;
				if (vx < V_MAX) vx += 2;
				bounce(pad_l);
			}
			// right paddle
			if (vx > 0 && bx + BALL >= PAD_XR && bx + BALL < PAD_XR + 5
			    && by + BALL > pad_r && by < pad_r + PAD_H)
			{
				bx16 = (PAD_XR - BALL) * 16;
				vx = -vx;
				if (vx > -V_MAX) vx -= 2;
				bounce(pad_r);
			}

			if (bx < -BALL) point_to(SIDE_RIGHT);		// left missed
			else if (bx > WORLD_W) point_to(SIDE_LEFT);	// right missed
			break;

		case G_POINT:
			if (--timer == 0)
			{
				if (score_l >= WIN_SCORE || score_r >= WIN_SCORE)
					gstate = G_OVER;
				else
					serve();
			}
			break;

		default:				// G_OVER
			if (start)
			{
				score_l = score_r = 0;
				serve();
			}
			break;
	}
	bx = bx16 >> 4;
	by = by16 >> 4;
}

int32_t main (void)
{
	uint8_t key, last_key = 0, new_key;
	uint8_t d[PKT_LEN];
	uint8_t quiet = LINK_LOST;	// frames since the last packet
	uint8_t start;			// serve pressed on either board
	uint8_t my_k5 = 0;		// RIGHT board : key 5 press counter
	uint8_t other_k5 = 0;		// LEFT board : last counter seen from the RIGHT board
	int16_t my_pad = (TOP + LCD_Ymax - PAD_H) / 2;

	UNLOCKREG();			// unlock register for programming
	DrvSYS_Open(48000000);		// set System Clock to run at 48MHz
	LOCKREG();			// lock register from programming

	close_seven_segment();
	DrvGPIO_Open(E_GPB, 11, E_IO_OUTPUT);	// buzzer kept off (no sound)
	DrvGPIO_SetBit(E_GPB, 11);		// output Hi = buzzer off
	init_LCD();
	clear_LCD();
	OpenKeyPad();			// initialize 3x3 keypad
	Init_UART1();

	pad_l = pad_r = my_pad;

	// ---- pick the side ----
	while (side == SIDE_NONE)
	{
		draw_menu();
		key = ScanKey();
		if (key == 1) side = SIDE_LEFT;
		if (key == 3) side = SIDE_RIGHT;
		DrvSYS_Delay(20000);
		frame++;
	}
	last_key = key;
	centre_ball();
	bx = bx16 >> 4;
	by = by16 >> 4;

	while (1)
	{
		frame++;

		// ---- own paddle and keys ----
		key = ScanKey();	// 1~9 while held, 0 = no key
		new_key = (key != last_key) ? key : 0;
		last_key = key;
		if (key == 2) my_pad -= PAD_STEP;
		if (key == 8) my_pad += PAD_STEP;
		my_pad = limit(my_pad, TOP, LCD_Ymax - PAD_H);
		start = (new_key == 5);
		if (start) my_k5++;

		// ---- packet from the other board ----
		if (quiet < LINK_LOST) quiet++;
		if (pkt_ready)
		{
			if (side == SIDE_LEFT && pkt_type == 'S')
			{
				pad_r = limit(pkt[0], TOP, LCD_Ymax - PAD_H);
				if (pkt[1] != other_k5)		// key 5 on the RIGHT board
				{
					other_k5 = pkt[1];
					start = 1;
				}
				quiet = 0;
			}
			else if (side == SIDE_RIGHT && pkt_type == 'M')
			{
				bx      = pkt[0];
				by      = pkt[1];
				pad_l   = pkt[2];
				score_l = pkt[4];
				score_r = pkt[5];
				gstate  = pkt[6] & 0x0F;
				scorer  = pkt[6] >> 4;
				quiet = 0;
			}
			else if (pkt_type == ((side == SIDE_LEFT) ? 'M' : 'S'))
				conflict = 1;			// both boards picked the same side
			pkt_ready = 0;
		}

		// ---- LEFT board runs the game, RIGHT board sends its paddle ----
		if (side == SIDE_LEFT)
		{
			pad_l = my_pad;
			game_step(quiet < LINK_LOST, start);
			d[0] = limit(bx, 0, 255);
			d[1] = limit(by, 0, LCD_Ymax - 1);
			d[2] = pad_l;
			d[3] = pad_r;
			d[4] = score_l;
			d[5] = score_r;
			d[6] = gstate | (scorer << 4);
			d[7] = 0;			// spare
			send_packet('M', d);
		}
		else
		{
			pad_r = my_pad;
			if (quiet >= LINK_LOST) gstate = G_WAIT;
			memset(d, 0, sizeof(d));
			d[0] = pad_r;
			d[1] = my_k5;
			send_packet('S', d);
		}

		draw_table();
		seg_score();		// 20 ms frame delay
	}
}
