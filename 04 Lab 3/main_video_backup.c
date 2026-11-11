//
// 1. Chan Li Kai
// 2. William Wong Xiao Kang
// 3. Irvin Chong Hou Ceng
// 4. Liang Yan Ee
//
// Lab 3 (video version) : play a video stored in flash on the LCD, no PC needed
//   video_data.c is made by : py lab3_video_to_lcd.py --video clip.mp4
//   to use : swap with main.c, add video_data.c to Keil and remove UART.c
//
//   video_data : each frame = XOR with the frame before, then PackBits
//                n = 0~127   : copy the next n+1 bytes
//                n = 129~255 : repeat the next byte 257-n times
//   VIDEO_HALF = 1 : video is 64x32, each pixel drawn as a 2x2 block
//                    (Keil MDK-Lite only links up to 32 KB)
//
//   Keypad : 1 = pause / play, 2 = restart, 4 = slower, 5 = normal, 6 = faster
//
#include <stdio.h>
#include <string.h>
#include "NUC1xx.h"
#include "GPIO.h"
#include "SYS.h"
#include "LCD.h"
#include "Seven_Segment.h"
#include "Scankey.h"
#include "video_data.h"

#define STEP_US		10000		// keypad checked every 10 ms while waiting
#define SPEED_STEP	20000
#define MAX_FRAME_US	300000

unsigned char screen[128 * 8];
#if VIDEO_HALF
uint8_t video_buf[VIDEO_W * VIDEO_H / 8];
#else
#define video_buf screen
#endif
const uint8_t *vp;			// next byte to read in VIDEO[]
uint16_t frame;
int32_t  frame_us = VIDEO_FRAME_US;
uint8_t  paused = 0;

// frames are stored as changes from a blank screen
void restart_video(void)
{
	memset(video_buf, 0, sizeof(video_buf));
	vp = VIDEO;
	frame = 0;
}

void decode_frame(void)
{
	uint16_t i = 0;
	uint16_t n;
	uint8_t  c, b;

	while (i < sizeof(video_buf))
	{
		c = *vp++;
		if (c < 128)				// copy c+1 bytes
		{
			for (n = c + 1; n > 0; n--)
				video_buf[i++] ^= *vp++;
		}
		else					// repeat next byte 257-c times
		{
			b = *vp++;
			for (n = 257 - c; n > 0; n--)
				video_buf[i++] ^= b;
		}
	}
}

#if VIDEO_HALF
// 4 bits -> 8 bits, every bit doubled (e.g. 0101 -> 00110011)
const uint8_t DOUBLE4[16] = {
	0x00, 0x03, 0x0C, 0x0F, 0x30, 0x33, 0x3C, 0x3F,
	0xC0, 0xC3, 0xCC, 0xCF, 0xF0, 0xF3, 0xFC, 0xFF
};

// 64x32 -> 128x64 : small page p, bits 0~3 -> big page 2p, bits 4~7 -> big page 2p+1
void scale_x2(void)
{
	uint8_t p, x, lo, hi, b;
	uint16_t top;

	for (p = 0; p < VIDEO_H / 8; p++)
	{
		top = (p * 2) * 128;
		for (x = 0; x < VIDEO_W; x++)
		{
			b  = video_buf[x + p * VIDEO_W];
			lo = DOUBLE4[b & 0x0F];
			hi = DOUBLE4[b >> 4];
			screen[top + 2 * x]           = lo;
			screen[top + 2 * x + 1]       = lo;
			screen[top + 128 + 2 * x]     = hi;
			screen[top + 128 + 2 * x + 1] = hi;
		}
	}
}
#endif

void do_key(uint8_t key)
{
	switch (key)
	{
		case 1:
			paused = !paused;
			if (paused) print_Line(0, "     PAUSE      ");
			break;
		case 2:
			restart_video();
			paused = 0;
			break;
		case 4:
			frame_us += SPEED_STEP;
			if (frame_us > MAX_FRAME_US) frame_us = MAX_FRAME_US;
			break;
		case 5:
			frame_us = VIDEO_FRAME_US;
			break;
		case 6:
			frame_us -= SPEED_STEP;
			if (frame_us < 0) frame_us = 0;
			break;
	}
}

int32_t main (void)
{
	uint8_t key;
	uint8_t last_key = 0;
	int32_t wait;

	UNLOCKREG();
	DrvSYS_Open(48000000);
	LOCKREG();

	close_seven_segment();
	init_LCD();
	clear_LCD();
	OpenKeyPad();

	restart_video();

	while (1)
	{
		if (!paused)
		{
			decode_frame();
#if VIDEO_HALF
			scale_x2();
#endif
			draw_LCD(screen);
			if (++frame >= VIDEO_FRAMES) restart_video();
		}

		// wait for the next frame, checking the keypad every 10 ms
		wait = frame_us;
		while (wait > 0 || paused)
		{
			key = ScanKey();
			if (key != 0 && last_key == 0)
			{
				do_key(key);
				last_key = key;
				break;
			}
			last_key = key;
			DrvSYS_Delay(STEP_US);
			if (!paused) wait -= STEP_US;
		}
	}
}
