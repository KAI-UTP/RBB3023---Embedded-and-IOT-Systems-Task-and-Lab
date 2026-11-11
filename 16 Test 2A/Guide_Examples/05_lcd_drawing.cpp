//
// Example 05 : LCD 128 x 32 (C12832) with lcd_c12832.h
//
//   Shows every drawing function:
//     text (scale 1 and 2), pixel, box (outline), fill_box (filled / erase)
//   Layout : big counter 00~99 on the left (scale 2, +1 every 0.5 s),
//            a bar that grows up on the right side,
//            small text + 3 boxes in the middle
//
//   LCD pins : MOSI p5, RESET p6, SCK p7, A0 p8, nCS p11
//   Coordinates : x 0 ~ 127 left -> right, y 0 ~ 31 top -> bottom
//
#include "mbed.h"
#include "lcd_c12832.h"

LCD_C12832 lcd;

int main()
{
	char text[24];
	int  num = 0;

	lcd.init();				// once, at the start

	while (true)
	{
		lcd.clear();			// 1. clear the buffer

		sprintf(text, "%02d", num);	// 2. draw into the buffer
		lcd.text(0, 0, text, 2);	//    scale 2 = 12 x 16 px per char
		lcd.text(0, 20, "count");	//    scale 1 = 6 x 8 px per char

		lcd.text(40, 0, "box  fill  dot");
		lcd.box(40, 12, 14, 14);		// outline
		lcd.fill_box(70, 12, 14, 14);		// filled
		lcd.fill_box(74, 16, 6, 6, false);	// erase a hole in it
		lcd.pixel(101, 18);			// one dot
		lcd.pixel(102, 18);

		int h = num % 33;			// bar height 0 ~ 32
		lcd.box(116, 0, 12, 32);		// bar frame
		lcd.fill_box(118, 32 - h, 8, h);	// grows upward

		lcd.update();			// 3. send the buffer to the LCD

		num = (num + 1) % 100;
		ThisThread::sleep_for(500ms);
	}
}
