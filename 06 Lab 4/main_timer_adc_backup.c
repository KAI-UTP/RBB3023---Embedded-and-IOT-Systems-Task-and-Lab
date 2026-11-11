//
// Lab 4 (starter) : Timer0 interrupt + ADC7 (VR1) shown on the LCD
//
//   Timer0    : periodic mode, 1 Hz, clock from 12MHz crystal
//               time out = (1/12MHz) * (255 + 1) * 46875 = 1 s
//               every interrupt toggles LED GPC12 and adds 1 to seconds
//   ADC7      : GPA7 = on-board variable resistor VR1, single-end, single mode
//               clock 22MHz / (1 + 1) = 11MHz, 12-bit result 0~4095 = 0~3.3V
//   LED       : GPC12 (0 = on)
//   LCD       : 128x64 via SPI3, print_Line(0~3, text) 16 chars per line
//   7-segment : turned off
//
#include <stdio.h>
#include "NUC1xx.h"
#include "GPIO.h"
#include "SYS.h"
#include "LCD.h"
#include "Seven_Segment.h"

#define ONESHOT		0	// count to TCMPR, interrupt, then stop
#define PERIODIC	1	// count to TCMPR, interrupt, start again from 0
#define TOGGLE		2	// like periodic, tout pin toggles
#define CONTINUOUS	3	// keep counting, interrupt when it passes TCMPR

volatile uint32_t seconds = 0;	// filled by the Timer0 interrupt
volatile uint8_t  led_state = 0;
volatile uint8_t  tick = 0;	// 1 = a new second, main loop updates the LCD

// Timer0 interrupt : once every second
void TMR0_IRQHandler(void)
{
	led_state = ~led_state;
	if (led_state) DrvGPIO_ClrBit(E_GPC, 12);	// LED on
	else           DrvGPIO_SetBit(E_GPC, 12);	// LED off
	seconds++;
	tick = 1;
	TIMER0->TISR.TIF = 1;		// write 1 to clear interrupt flag
}

void InitTIMER0(void)
{
	/* Step 1. Enable and select Timer clock source */
	SYSCLK->CLKSEL1.TMR0_S = 0;	// 12MHz crystal for Timer0
	SYSCLK->APBCLK.TMR0_EN = 1;	// enable Timer0 clock
	/* Step 2. Select operation mode */
	TIMER0->TCSR.MODE = PERIODIC;
	/* Step 3. Time out period = (1/12MHz) * (PRESCALE + 1) * TCMPR */
	TIMER0->TCSR.PRESCALE = 255;	// prescale 0~255
	TIMER0->TCMPR = 46875;		// TCMPR 0~16777215 -> 1 s
	/* Step 4. Enable interrupt */
	TIMER0->TCSR.IE = 1;
	TIMER0->TISR.TIF = 1;		// write 1 to clear for safety
	NVIC_EnableIRQ(TMR0_IRQn);
	/* Step 5. Enable Timer module */
	TIMER0->TCSR.CRST = 1;		// reset up counter
	TIMER0->TCSR.CEN = 1;		// start Timer0
}

void InitADC(void)
{
	/* Step 1. GPIO initial */
	GPIOA->OFFD |= 0x00800000;		// disable GPA7 digital input path
	SYS->GPAMFP.ADC7_SS21_AD6 = 1;		// GPA7 = ADC7
	/* Step 2. ADC clock source, then enable ADC */
	SYSCLK->CLKSEL1.ADC_S = 2;		// 22MHz for ADC
	SYSCLK->CLKDIV.ADC_N = 1;		// ADC clock = 22MHz / 2 = 11MHz
	SYSCLK->APBCLK.ADC_EN = 1;		// enable ADC clock
	ADC->ADCR.ADEN = 1;			// enable ADC module
	/* Step 3. Operation mode */
	ADC->ADCR.DIFFEN = 0;			// single-end input
	ADC->ADCR.ADMD = 0;			// single mode
	/* Step 4. Channel */
	ADC->ADCHER.CHEN = 0x80;		// channel 7 only
	/* Step 5. Clear flag (polling, no interrupt) */
	ADC->ADSR.ADF = 1;
	/* Step 6. Start conversion */
	ADC->ADCR.ADST = 1;
}

// read ADC7 : wait for the conversion, take the result, start the next one
uint16_t read_adc7(void)
{
	uint16_t value;

	while (ADC->ADSR.ADF == 0);		// wait till conversion done
	ADC->ADSR.ADF = 1;			// write 1 to clear the flag
	value = ADC->ADDR[7].RSLT;
	ADC->ADCR.ADST = 1;			// start next conversion
	return value;
}

int32_t main (void)
{
	char TEXT[17];
	uint16_t adc;
	uint32_t mv;

	UNLOCKREG();			// unlock register for programming
	DrvSYS_Open(48000000);		// set System Clock to run at 48MHz
	LOCKREG();			// lock register from programming

	close_seven_segment();
	DrvGPIO_Open(E_GPC, 12, E_IO_OUTPUT);
	DrvGPIO_SetBit(E_GPC, 12);	// LED off

	init_LCD();
	clear_LCD();
	print_Line(0, "Lab 4 Timer ADC ");

	InitADC();
	InitTIMER0();

	while (1)
	{
		adc = read_adc7();
		mv  = (uint32_t)adc * 3300 / 4095;	// 0~4095 -> 0~3300 mV

		sprintf(TEXT, "ADC7 : %4d     ", adc);
		print_Line(1, TEXT);
		sprintf(TEXT, "Volt : %d.%03d V  ", mv / 1000, mv % 1000);
		print_Line(2, TEXT);
		if (tick)
		{
			tick = 0;
			sprintf(TEXT, "Time : %5d s  ", seconds);
			print_Line(3, TEXT);
		}
		DrvSYS_Delay(100000);		// update about 10 times a second
	}
}
