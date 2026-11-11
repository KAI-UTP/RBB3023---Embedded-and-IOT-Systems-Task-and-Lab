//
// Lab 7 : UART (lecture EIOTS_10 UART, Smpl_UART0)
//
// Two learning boards, both running this same code:
//   TX : main loop sends the 8-byte string "NuMicro0" ~ "NuMicro9", one every 1 s
//   RX : UART0 interrupt collects 8 bytes, main loop prints them on the LCD
//
// Fixes compared with the lecture code:
//   - ISR stores a byte before checking for 8, so no byte is lost (the lecture
//     dropped every 9th byte, which shifted the text)
//   - callback takes uint32_t like PFN_DRVUART_CALLBACK expects
//   - LCD text buffers are 17 bytes (16 chars + '\0')
//   - DrvSYS_Delay max is ~349 ms at 48 MHz, so the 1 s wait is a loop
//   - LCD printing is done in main, not inside the interrupt
//
// Nu-LB-NUC140, UART0 9600 baud 8N1
//   pin32 GPB0/RX0 to the other board's TX0 (GPB1)
//   pin33 GPB1/TX0 to the other board's RX0 (GPB0)
//   GND to GND
//
#include <stdio.h>
#include "NUC1xx.h"
#include "GPIO.h"
#include "SYS.h"
#include "UART.h"
#include "LCD.h"

#define MSG_LEN 8

volatile uint8_t comRbuf[MSG_LEN + 1];	// bytes being received
volatile uint8_t comRbytes = 0;
volatile uint8_t rx_line[MSG_LEN + 1];	// last full message
volatile uint8_t rx_ready = 0;

// UART0 interrupt : read every byte waiting in the FIFO
void UART_INT_HANDLE(uint32_t u32IntStatus)
{
	uint8_t i;
	uint8_t bInChar[1];

	while (UART0->ISR.RDA_IF == 1)
	{
		DrvUART_Read(UART_PORT0, bInChar, 1);
		comRbuf[comRbytes] = bInChar[0];
		comRbytes++;
		if (comRbytes == MSG_LEN)		// full message received
		{
			for (i = 0; i < MSG_LEN; i++) rx_line[i] = comRbuf[i];
			rx_line[MSG_LEN] = '\0';
			comRbytes = 0;
			rx_ready = 1;
		}
	}
}

void Init_UART0(void)
{
	STR_UART_T sParam;

	DrvGPIO_InitFunction(E_FUNC_UART0);		// GPB0 = RX0, GPB1 = TX0

	sParam.u32BaudRate       = 9600;
	sParam.u8cDataBits       = DRVUART_DATABITS_8;
	sParam.u8cStopBits       = DRVUART_STOPBITS_1;
	sParam.u8cParity         = DRVUART_PARITY_NONE;
	sParam.u8cRxTriggerLevel = DRVUART_FIFO_1BYTES;
	DrvUART_Open(UART_PORT0, &sParam);

	DrvUART_EnableInt(UART_PORT0, DRVUART_RDAINT, UART_INT_HANDLE);
}

int32_t main(void)
{
	uint8_t i = 0;
	uint8_t t;
	uint8_t dataout[MSG_LEN + 1] = "NuMicro0";
	char TEXT1[17];
	char TEXT2[17];

	UNLOCKREG();
	DrvSYS_Open(48000000);
	LOCKREG();

	init_LCD();
	clear_LCD();
	print_Line(0, "Lab7 Smpl_UART0 ");
	print_Line(2, "RX: waiting...  ");

	Init_UART0();

	while (1)
	{
		// send one message
		dataout[7] = '0' + i;
		DrvUART_Write(UART_PORT0, dataout, MSG_LEN);
		sprintf(TEXT1, "TX: %s    ", dataout);
		print_Line(1, TEXT1);
		i++;
		if (i >= 10) i = 0;

		// wait 1 s, showing anything received meanwhile
		for (t = 0; t < 100; t++)
		{
			if (rx_ready)
			{
				rx_ready = 0;
				sprintf(TEXT2, "RX: %s    ", (char *)rx_line);
				print_Line(2, TEXT2);
			}
			DrvSYS_Delay(10000);		// 10 ms
		}
	}
}
