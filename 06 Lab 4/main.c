//
// Smpl_Timer_LED + 7-segment : every second the LED turns on/off and
//                              the 7-segment counts 0 ~ 255, then back to 0
//
//   Timer0    : periodic mode, clock from 12MHz crystal
//               time out = (1/12MHz) * (255 + 1) * 46875 = 1 s
//               TMR0_IRQHandler toggles the LED and adds 1 to count
//   LED       : GPC12 (0 = on)
//   7-segment : GPC4~7 select digit (1 = on), GPE0~7 segments (0 = on)
//               right 3 digits show count with leading zeros (000 ~ 255)
//   HCLK      : 12MHz crystal (as in the lecture sample)
//
#include <stdio.h>
#include "NUC1xx.h"
#include "SYS.h"
#include "GPIO.h"
#include "Seven_Segment.h"

#define ONESHOT		0	// counting and interrupt when reach TCMPR number, then stop
#define PERIODIC	1	// counting and interrupt when reach TCMPR number, then counting from 0 again
#define TOGGLE		2	// keep counting and interrupt when reach TCMPR number, tout toggled (between 0 and 1)
#define CONTINUOUS	3	// keep counting and interrupt when reach TCMPR number

volatile uint32_t ledState = 0;
volatile uint16_t count = 0;	// number on the 7-segment, 0 ~ 255

// Timer0 interrupt : every 1 s
void TMR0_IRQHandler(void)
{
	ledState = ~ledState;			// changing ON/OFF state
	if (ledState) DrvGPIO_ClrBit(E_GPC, 12);	// LED on
	else          DrvGPIO_SetBit(E_GPC, 12);	// LED off

	count++;				// next number
	if (count > 255) count = 0;		// after 255 start again from 0

	TIMER0->TISR.TIF = 1;			// clear interrupt flag
}

void InitTIMER0(void)
{
	/* Step 1. Enable and select Timer clock source */
	SYSCLK->CLKSEL1.TMR0_S = 0;	// select 12MHz for Timer0 clock source
	SYSCLK->APBCLK.TMR0_EN = 1;	// enable Timer0 clock source
	/* Step 2. Select operation mode */
	TIMER0->TCSR.MODE = PERIODIC;	// select operation mode
	/* Step 3. Time out period = (period of timer clock input) * (8-bit prescale + 1) * (24-bit TCMP) */
	TIMER0->TCSR.PRESCALE = 255;	// set prescale [0~255]
	TIMER0->TCMPR = 46875;		// set TCMPR [0~16777215]
					// (1/12MHz) * (255 + 1) * 46875 = 1 s
	/* Step 4. Enable interrupt */
	TIMER0->TCSR.IE = 1;
	TIMER0->TISR.TIF = 1;		// write 1 to clear for safety
	NVIC_EnableIRQ(TMR0_IRQn);	// enable Timer0 interrupt
	/* Step 5. Enable Timer module */
	TIMER0->TCSR.CRST = 1;		// reset up counter
	TIMER0->TCSR.CEN = 1;		// enable Timer0
}

// show 0 ~ 255 on the right 3 digits, always 3 digits (000 ~ 255)
void seg_display(uint16_t value)
{
	close_seven_segment();
	show_seven_segment(2, value / 100);		// hundreds
	DrvSYS_Delay(5000);

	close_seven_segment();
	show_seven_segment(1, (value / 10) % 10);	// tens
	DrvSYS_Delay(5000);

	close_seven_segment();
	show_seven_segment(0, value % 10);				// ones
	DrvSYS_Delay(5000);
}

int32_t main (void)
{
	UNLOCKREG();
	SYSCLK->PWRCON.XTL12M_EN = 1;	// enable 12MHz crystal
	DrvSYS_Delay(5000);		// wait for 12MHz crystal stable
	SYSCLK->CLKSEL0.HCLK_S = 0;	// HCLK = 12MHz crystal
	LOCKREG();

	DrvGPIO_Open(E_GPC, 12, E_IO_OUTPUT);	// set GPC12 output for LED
	DrvGPIO_SetBit(E_GPC, 12);		// LED off
	InitTIMER0();				// set Timer ticking

	while (1)
	{
		seg_display(count);	// keep the 7-segment lit, count changes in the interrupt
	}
}
