#define F_CPU 8000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include "lcd.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <avr/interrupt.h>

void init_lcd() {
	DDRD = _BV(4);
	TCCR1A = _BV(COM1B1) | _BV(WGM10);
	TCCR1B = _BV(WGM12) | _BV(CS11);
	OCR1B = 128;

	lcd_init(LCD_DISP_ON);
	lcd_clrscr();
}

void init_adc() {
	ADMUX = _BV(REFS0);
	ADCSRA = _BV(ADEN) | _BV(ADPS1) | _BV(ADPS0);
}

void writeLCD(uint16_t adc) {
	lcd_clrscr();
	
	lcd_puts("Muscle");
	lcd_gotoxy(12, 0);
	char adcStr[16];
	itoa(adc, adcStr, 10);
	lcd_puts(adcStr);
	
	lcd_gotoxy(0, 1);
	if(adc > 600)	lcd_puts("contraction");
	else lcd_puts("relaxation");
	
	_delay_ms(100);
	lcd_home();

}

int main() {
	init_lcd();
	init_adc();
	
	while (1) {
		ADCSRA |= _BV(ADSC);
		while (!(ADCSRA & _BV(ADIF)));
		writeLCD(ADC);
		_delay_ms(100);
	}
	

	return 0;
}
