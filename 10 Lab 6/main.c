//
// Lab 6 : 5-channel ADC = 4 channels by interrupt (I) + 1 channel by polling (P)
//
//   Channels : CH1 = ADC4 (GPA4)  I        CH4 = ADC7 (GPA7) = VR1  I
//              CH2 = ADC5 (GPA5)  I        CH5 = ADC3 (GPA3)        P
//              CH3 = ADC6 (GPA6)  I
//              12-bit result 0 ~ 4095  =  0 ~ 3.3V  (Vref = 3.3V on the board)
//              only VR1 is on the board, the others float (random values)
//              until you wire a voltage 0 ~ 3.3V to the pin (header ADC0~7)
//   ADC clk  : 22MHz RC / (1 + 1) = 11MHz (max 16MHz)
//
//   One ADC, so every loop the two methods take turns:
//     I part : continuous scan of ADC4 ~ 7 (0xF0) with ADC interrupt on,
//              BSP ADC_IRQHandler calls AdcIntCallback after each scan,
//              wait for 2 callbacks (one full fresh scan), read 4 channels
//     P part : stop the scan, interrupt off, single mode on ADC3 only,
//              start (ADST = 1) -> wait ADF = 1 -> clear ADF -> read ADDR[3]
//
//   Keypad   : key 1 ~ 5 = show that one channel (value, voltage, percent)
//              key 6     = show all 5 channels together
//   Pin share: keypad rows GPA0 ~ 2, columns GPA3 ~ 5, but GPA3 / 4 / 5 are
//              also ADC3 / 4 / 5. key_scan() switches them to GPIO for the
//              scan only, then back to ADC. ADC is always read after that.
//              A column held low by a wire is ignored (not a key press).
//
#include <stdio.h>
#include "NUC1xx.h"
#include "SYS.h"
#include "GPIO.h"
#include "ADC.h"
#include "LCD.h"
#include "Scankey.h"

#define IRQ_MASK	0xF0	// ADC4 ~ ADC7 by interrupt
#define IRQ_FIRST	4	// CH1 = ADC4
#define P_CH		3	// CH5 = ADC3 by polling
#define P_MASK		0x08

volatile uint8_t gu8AdcIntFlag = 0;	// set by the ADC callback

// ADC interrupt callback (called from BSP ADC_IRQHandler)
void AdcIntCallback(uint32_t u32UserData)
{
	gu8AdcIntFlag = 1;
}

// ---------------- pins ----------------

// GPA3 / 4 / 5 to ADC : input (no pull-up), digital path off, ADC function
void pins_adc(void)
{
	DrvGPIO_Open(E_GPA, 3, E_IO_INPUT);
	DrvGPIO_Open(E_GPA, 4, E_IO_INPUT);
	DrvGPIO_Open(E_GPA, 5, E_IO_INPUT);
	GPIOA->OFFD |= 0x00380000;
	SYS->GPAMFP.ADC3_AD10 = 1;		// GPA3 = ADC3
	SYS->GPAMFP.ADC4_AD9 = 1;		// GPA4 = ADC4
	SYS->GPAMFP.ADC5_AD8 = 1;		// GPA5 = ADC5
}

// GPA3 / 4 / 5 to keypad columns : GPIO, digital path on, quasi (pull-up)
void pins_keypad(void)
{
	SYS->GPAMFP.ADC3_AD10 = 0;
	SYS->GPAMFP.ADC4_AD9 = 0;
	SYS->GPAMFP.ADC5_AD8 = 0;
	GPIOA->OFFD &= ~0x00380000;
	DrvGPIO_Open(E_GPA, 3, E_IO_QUASI);
	DrvGPIO_Open(E_GPA, 4, E_IO_QUASI);
	DrvGPIO_Open(E_GPA, 5, E_IO_QUASI);
}

void adc_pin_init(void)
{
	OpenKeyPad();				// GPA0 ~ 5 quasi
	pins_adc();				// GPA3 / 4 / 5 to ADC
	DrvGPIO_Open(E_GPA, 6, E_IO_INPUT);
	DrvGPIO_Open(E_GPA, 7, E_IO_INPUT);
	GPIOA->OFFD |= 0x00C00000;		// digital path off for GPA6 / 7
	SYS->GPAMFP.ADC6_AD7 = 1;		// GPA6 = ADC6
	SYS->GPAMFP.ADC7_SS21_AD6 = 1;		// GPA7 = ADC7 (VR1)
}

// ---------------- keypad ----------------

void key_delay(void)
{
	volatile int j;
	for (j = 0; j < 1000; j++);
}

// same numbering as BSP ScanKey() : 1 ~ 9, 0 = no key
//   row GPA2 low -> 1 / 4 / 7, GPA1 low -> 2 / 5 / 8, GPA0 low -> 3 / 6 / 9
//   column GPA3 -> 1 ~ 3, GPA4 -> 4 ~ 6, GPA5 -> 7 ~ 9
uint8_t key_scan(void)
{
	uint8_t row, col, ext = 0, key = 0;

	pins_keypad();
	for (row = 0; row < 6; row++)
		DrvGPIO_SetBit(E_GPA, row);	// all rows / columns high
	key_delay();

	// a column already low with no row driven = wired to something, skip it
	for (col = 0; col < 3; col++)
		if (DrvGPIO_GetBit(E_GPA, 3 + col) == 0) ext |= 1 << col;

	for (row = 0; row < 3 && key == 0; row++)
	{
		DrvGPIO_ClrBit(E_GPA, 2 - row);	// drive one row low
		key_delay();
		for (col = 0; col < 3; col++)
		{
			if ((ext & (1 << col)) == 0 && DrvGPIO_GetBit(E_GPA, 3 + col) == 0)
			{
				key = row + 1 + col * 3;
				break;
			}
		}
		DrvGPIO_SetBit(E_GPA, 2 - row);
	}

	pins_adc();
	return key;
}

// ---------------- ADC ----------------

// stop the ADC and wait till the conversion in progress is finished
void adc_stop(void)
{
	ADC->ADCR.ADST = 0;
	while (ADC->ADSR.BUSY == 1);
}

// once at start : BSP driver opens the ADC and registers the callback
void InitADC(void)
{
	DrvADC_Open(ADC_SINGLE_END, ADC_CONTINUOUS_OP, IRQ_MASK, INTERNAL_RC22MHZ, 1);
	DrvADC_EnableADCInt(AdcIntCallback, 0);
}

// I part : continuous scan of ADC4 ~ 7, interrupt on, read 4 channels
void read_irq(uint16_t *value)
{
	int i;

	adc_stop();
	ADC->ADCR.ADMD = 3;			// continuous scan
	ADC->ADCHER.CHEN = IRQ_MASK;		// ADC4 ~ 7
	ADC->ADSR.ADF = 1;			// clear old flag
	gu8AdcIntFlag = 0;
	ADC->ADCR.ADIE = 1;			// ADC interrupt on
	NVIC_EnableIRQ(ADC_IRQn);
	ADC->ADCR.ADST = 1;			// start scanning

	while (gu8AdcIntFlag == 0);		// 1st callback
	gu8AdcIntFlag = 0;
	while (gu8AdcIntFlag == 0);		// 2nd callback = one full fresh scan
	for (i = 0; i < 4; i++)
		value[i] = DrvADC_GetConversionData(IRQ_FIRST + i);
}

// P part : interrupt off, single mode on ADC3, poll the flag
uint16_t read_polling(void)
{
	adc_stop();
	ADC->ADCR.ADIE = 0;			// ADC interrupt off
	NVIC_DisableIRQ(ADC_IRQn);
	NVIC_ClearPendingIRQ(ADC_IRQn);
	ADC->ADCR.ADMD = 0;			// single mode
	ADC->ADCHER.CHEN = P_MASK;		// ADC3 only
	ADC->ADSR.ADF = 1;			// clear the flag

	ADC->ADCR.ADST = 1;			// start one conversion
	while (ADC->ADSR.ADF == 0);		// wait till conversion done
	ADC->ADSR.ADF = 1;			// write 1 to clear the flag
	return ADC->ADDR[P_CH].RSLT;
}

// ---------------- LCD ----------------

// key 1 ~ 5 : one channel, big detail
void show_one(uint8_t ch, uint16_t value)
{
	char TEXT[17];				// 16 chars + end zero
	uint32_t mv = value * 3300 / 4095;
	uint8_t adc_no = (ch == 5) ? P_CH : IRQ_FIRST + ch - 1;

	sprintf(TEXT, "CH%d = ADC%d %s  ", ch, adc_no, (ch == 4) ? "VR1" : "   ");
	TEXT[15] = (ch == 5) ? 'P' : 'I';	// how this channel is read
	print_Line(0, TEXT);
	sprintf(TEXT, "ADC Value: %4d ", value);
	print_Line(1, TEXT);
	sprintf(TEXT, "Volt : %d.%03d V ", mv / 1000, mv % 1000);
	print_Line(2, TEXT);
	sprintf(TEXT, "Level: %3d %%    ", value * 100 / 4095);
	print_Line(3, TEXT);
}

// key 6 : all 5 channels, I = interrupt, P = polling
void show_all(uint16_t *value)
{
	char TEXT[17];
	uint32_t mv = value[4] * 3300 / 4095;

	print_Line(0, "IRQ x4 + Poll x1");
	sprintf(TEXT, "1I%4d  2I%4d  ", value[0], value[1]);
	print_Line(1, TEXT);
	sprintf(TEXT, "3I%4d  4I%4d  ", value[2], value[3]);
	print_Line(2, TEXT);
	sprintf(TEXT, "5P%4d  %d.%03dV ", value[4], mv / 1000, mv % 1000);
	print_Line(3, TEXT);
}

// ---------------- main ----------------

int32_t main(void)
{
	uint8_t view = 6, key, last_key = 0;
	uint16_t adc[5];			// [0 ~ 3] = CH1 ~ 4 (I), [4] = CH5 (P)

	UNLOCKREG();
	DrvSYS_Open(48000000);
	LOCKREG();

	init_LCD();
	clear_LCD();
	adc_pin_init();
	InitADC();

	while (1)
	{
		// keypad : 1 ~ 5 one channel, 6 all channels (new press only)
		key = key_scan();
		if (key != 0 && last_key == 0 && key <= 6 && key != view)
		{
			view = key;
			clear_LCD();
		}
		last_key = key;

		read_irq(adc);			// CH1 ~ 4 by interrupt
		adc[4] = read_polling();	// CH5 by polling

		if (view == 6)	show_all(adc);
		else		show_one(view, adc[view - 1]);

		DrvSYS_Delay(50000);		// 50 ms
	}
}
