//
// 1. Chan Li Kai
// 2. William Wong Xiao Kang
// 3. Irvin Chong Hou Ceng
// 4. Liang Yan Ee
//
// Lab 3 : live webcam on the LCD over UART
//         laptop (lab3_cam_live.py) -> CH340 USB-TTL -> UART1 -> LCD
//
//   frame from PC : 0xAA 0x55 + 1024 bytes (128x64, buffer[x + page*128], bit 1 = dark)
//   reply to PC   : 'K' = frame received, '1'~'9' = keypad key
//
//   UART1  : 460800 baud 8N1, GPB4 = RX1 (JP5 pin 5), GPB5 = TX1 (JP5 pin 6), GND JP5 pin 17
//   Keypad : GPA0~5, LCD : SPI3
//
#include <stdio.h>
#include <string.h>
#include "NUC1xx.h"
#include "GPIO.h"
#include "SYS.h"
#include "UART.h"
#include "LCD.h"
#include "Seven_Segment.h"
#include "Scankey.h"

#define BAUD_RATE	460800
#define FRAME_BYTES	1024		// 128 x 64 / 8
#define LOST_MS		2000		// no frame for 2 s -> show "PC lost"

unsigned char screen[FRAME_BYTES];

// filled by the UART1 interrupt
volatile uint8_t  rx_frame[FRAME_BYTES];
volatile uint16_t rx_count = 0;
volatile uint8_t  rx_state = 0;		// 0 = wait 0xAA, 1 = wait 0x55, 2 = frame data
volatile uint8_t  frame_ready = 0;	// 1 = rx_frame[] full, not taken by main loop yet

// read every byte in the FIFO and build the frame
void UART1_INT_HANDLE(uint32_t u32IntStatus)
{
	uint8_t b;

	while (UART1->FSR.RX_EMPTY == 0)
	{
		b = UART1->DATA;
		switch (rx_state)
		{
			case 0:
				if (b == 0xAA) rx_state = 1;
				break;
			case 1:
				if (b == 0x55)
				{
					rx_state = 2;
					rx_count = 0;
				}
				else if (b != 0xAA)
					rx_state = 0;
				break;
			default:
				if (!frame_ready) rx_frame[rx_count] = b;
				if (++rx_count >= FRAME_BYTES)
				{
					frame_ready = 1;
					rx_state = 0;
				}
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

void send_byte(uint8_t c)
{
	DrvUART_Write(UART_PORT1, &c, 1);
}

void show_waiting(char *line3)
{
	clear_LCD();
	print_Line(0, "Lab 3 : UART cam");
	print_Line(1, "UART1 460800    ");
	print_Line(2, "JP5 p5 RX p6 TX ");
	print_Line(3, line3);
}

int32_t main (void)
{
	uint8_t  key;
	uint8_t  last_key = 0;
	uint32_t idle_ms = 0;		// time since the last frame
	uint32_t frames = 0;
	uint16_t last_count = 0;
	uint16_t stall_ms = 0;		// time a half frame has not grown

	UNLOCKREG();
	DrvSYS_Open(48000000);
	LOCKREG();

	close_seven_segment();
	init_LCD();
	OpenKeyPad();
	Init_UART1();

	show_waiting("Waiting for PC..");

	while (1)
	{
		if (frame_ready)
		{
			memcpy(screen, (const void *)rx_frame, FRAME_BYTES);
			frame_ready = 0;
			send_byte('K');		// PC sends the next frame while the LCD draws
			draw_LCD(screen);
			frames++;
			idle_ms = 0;
		}
		else
		{
			DrvSYS_Delay(1000);	// 1 ms

			// half a frame then 50 ms of no bytes : drop it, wait for 0xAA 0x55 again
			if (rx_state == 2 && rx_count == last_count) stall_ms++;
			else stall_ms = 0;
			last_count = rx_count;
			if (stall_ms >= 50)
			{
				rx_state = 0;
				stall_ms = 0;
			}

			if (idle_ms < LOST_MS) idle_ms++;
			else if (idle_ms == LOST_MS && frames > 0)
			{
				show_waiting("PC lost, waiting");
				idle_ms++;
			}
		}

		// send each key press to the PC once (not while held)
		key = ScanKey();
		if (key != 0 && last_key == 0)
			send_byte('0' + key);
		last_key = key;
	}
}
