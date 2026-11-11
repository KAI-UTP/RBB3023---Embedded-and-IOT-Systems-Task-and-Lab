//
// Lab 6 : Smpl_ADC_VR1 + Smpl_ADC_IRQ in one program, press any key to switch
//
//   VR1     : on-board variable resistor on GPA7 = ADC7
//             12-bit result 0 ~ 4095  =  0 ~ 3.3V  (Vref = 3.3V on the board)
//   ADC clk : 22MHz RC / (1 + 1) = 11MHz (max 16MHz)
//
//   Mode 1  : Smpl_ADC_VR1 (polling, register level)
//             single mode, channel 7 only, interrupt off
//             start (ADST = 1) -> wait ADF = 1 -> clear ADF -> read ADDR[7]
//   Mode 2  : Smpl_ADC_IRQ (interrupt, BSP driver)
//             DrvADC_Open continuous scan of channels 0~7 (0xFF)
//             BSP ADC_IRQHandler calls AdcIntCallback after each scan,
//             callback sets gu8AdcIntFlag = 1 and counts interrupts
//             main waits for the flag, then reads ADC7
//
//   LCD     : line 0 = mode name, line 1 = ADC7 value,
//             line 2 = voltage, line 3 = percent (mode 1) / IRQ count (mode 2)
//   Keypad  : GPA0~5, any key switches mode 1 <-> mode 2
//             (ADC0~5 share these pins, only ADC7 is shown)
//
#include <stdio.h>
#include "NUC1xx.h"
#include "SYS.h"
#include "GPIO.h"
#include "ADC.h"
#include "LCD.h"
#include "Scankey.h"

volatile uint8_t gu8AdcIntFlag = 0;	// set by the ADC callback
volatile uint32_t irq_count = 0;	// number of ADC interrupts

// ADC interrupt callback (called from BSP ADC_IRQHandler)
void AdcIntCallback(uint32_t u32UserData)
{
	gu8AdcIntFlag = 1;
	irq_count++;
}

// GPA7 as analog input for VR1
void adc_pin_init(void)
{
	GPIOA->OFFD |= 0x00800000;		// disable digital input path of GPA7
	SYS->GPAMFP.ADC7_SS21_AD6 = 1;		// GPA7 = ADC7
}

// Mode 1 : Smpl_ADC_VR1 setup, register level
void InitADC(void)
{
	/* stop mode 2 if it was running */
	ADC->ADCR.ADST = 0;
	ADC->ADCR.ADIE = 0;
	NVIC_DisableIRQ(ADC_IRQn);
	/* Step 2. ADC clock source, then enable ADC */
	SYSCLK->CLKSEL1.ADC_S = 3;		// 3 = 22MHz RC (2 = HCLK, too fast at 48MHz)
	SYSCLK->CLKDIV.ADC_N = 1;		// ADC clock = 22MHz / 2 = 11MHz
	SYSCLK->APBCLK.ADC_EN = 1;		// enable ADC clock
	ADC->ADCR.ADEN = 1;			// enable ADC module
	/* Step 3. operation mode */
	ADC->ADCR.DIFFEN = 0;			// single-end input
	ADC->ADCR.ADMD = 0;			// single mode
	/* Step 4. channel */
	ADC->ADCHER.CHEN = 0x80;		// channel 7 only (bit 7)
	/* Step 5. clear the flag (polling, so no interrupt) */
	ADC->ADSR.ADF = 1;
	/* Step 6. start the first conversion */
	ADC->ADCR.ADST = 1;
}

// Mode 2 : Smpl_ADC_IRQ setup, BSP driver
void InitADC_IRQ(void)
{
	// open 8 ADC channels for continuous sampling, 22MHz / (1 + 1) = 11MHz
	DrvADC_Open(ADC_SINGLE_END, ADC_CONTINUOUS_OP, 0xFF, INTERNAL_RC22MHZ, 1);
	gu8AdcIntFlag = 0;
	irq_count = 0;
	DrvADC_EnableADCInt(AdcIntCallback, 0);
	DrvADC_StartConvert();
}

// Mode 1 : wait for one conversion, read it, start the next one
uint16_t read_vr1_polling(void)
{
	uint16_t value;

	while (ADC->ADSR.ADF == 0);		// wait till conversion done
	ADC->ADSR.ADF = 1;			// write 1 to clear the flag
	value = ADC->ADDR[7].RSLT;
	ADC->ADCR.ADST = 1;			// start the next conversion
	return value;
}

// Mode 2 : wait for the callback flag, then read ADC7
uint16_t read_vr1_irq(void)
{
	while (gu8AdcIntFlag == 0);		// wait till the callback sets it
	gu8AdcIntFlag = 0;
	return DrvADC_GetConversionData(7);
}

// show the selected mode on LCD line 0
void show_title(uint8_t mode)
{
	clear_LCD();
	if (mode == 1)	print_Line(0, "1:Smpl_ADC_VR1  ");
	else		print_Line(0, "2:Smpl_ADC_IRQ  ");
}

int32_t main(void)
{
	char TEXT[17];				// 16 chars + end zero
	uint8_t mode = 1, key, last_key = 0;
	uint16_t adc;
	uint32_t mv;

	UNLOCKREG();
	DrvSYS_Open(48000000);
	LOCKREG();

	init_LCD();
	OpenKeyPad();
	adc_pin_init();
	InitADC();
	show_title(mode);

	while (1)
	{
		// any key (new press only) switches the mode
		key = ScanKey();
		if (key != 0 && last_key == 0)
		{
			if (mode == 1)	{ mode = 2; InitADC_IRQ(); }
			else		{ mode = 1; InitADC(); }
			show_title(mode);
		}
		last_key = key;

		if (mode == 1)	adc = read_vr1_polling();
		else		adc = read_vr1_irq();

		mv = adc * 3300 / 4095;		// 0 ~ 3300 mV
		sprintf(TEXT, "ADC7 : %4d     ", adc);
		print_Line(1, TEXT);
		sprintf(TEXT, "Volt : %d.%03d V ", mv / 1000, mv % 1000);
		print_Line(2, TEXT);
		if (mode == 1)	sprintf(TEXT, "Level: %3d %%    ", adc * 100 / 4095);
		else		sprintf(TEXT, "IRQ: %-11u", irq_count);
		print_Line(3, TEXT);

		DrvSYS_Delay(30000);		// 30 ms
	}
}
