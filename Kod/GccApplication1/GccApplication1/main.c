#define F_CPU 7372800UL
#define BAUD 9600UL
#define BAUDRATE (F_CPU/16UL/BAUD-1)
#define RCV_BUFFER_SIZE 512 

#include <avr/io.h>
#include <stdio.h>
#include <util/delay.h>
#include <stdlib.h>
#include <inttypes.h>

#include <avr/interrupt.h>
#include "lcd.h"
#include <string.h>

uint32_t rcvParameter;
uint16_t rcvResponse;

int rcvBufferStart=0;
int rcvBufferEnd=0;
char rcvBuffer[RCV_BUFFER_SIZE];


ISR(USART_RXC_vect){
	rcvBuffer[rcvBufferEnd] = UDR;
	rcvBufferEnd++;
	if(rcvBufferEnd == RCV_BUFFER_SIZE){
		rcvBufferEnd = 0;
	}
}

int rcvBufferSize(){
	if(rcvBufferEnd >= rcvBufferStart){
		return rcvBufferEnd - rcvBufferStart;
	}else{
		return RCV_BUFFER_SIZE - rcvBufferStart + rcvBufferEnd;
	}
}

void serialSendChar(unsigned char data){
	while(!(UCSRA&(1 << UDRE)));
	UDR = data;
	while(!(UCSRA&(1 << TXC)));
}

void sendCommand(uint16_t command, uint32_t parameter){
	char cmd[12];
	cmd[0] = 0x55;
	cmd[1] = 0xaa;

	// Device ID
	cmd[2] = 0x01;
	cmd[3] = 0x00;

	// Parameter
	cmd[4] = parameter & 0xff;
	cmd[5] = (parameter >> 8) & 0xff;
	cmd[6] = (parameter >> 16) & 0xff;
	cmd[7] = (parameter >> 24) & 0xff;

	// Command
	cmd[8] = command & 0x00FF;
	cmd[9] = command >> 8;

	uint16_t checksum = 0;
	for(int i=0; i < 10; i++){
		checksum = checksum + cmd[i];
	}

	cmd[10] = checksum & 0x00FF;
	cmd[11] = checksum >> 8;

	// 1 packet = 12 bytes
	for(int i=0; i < 12; i++){
		serialSendChar(cmd[i]);
	}
}

void receiveAck(){
	char msg[12];
	uint16_t checksum = 0;
	
	while(rcvBufferSize() < 12){
		_delay_ms(1);
	}
	
	for(int i=0; i < 12; i++){
		msg[i] = rcvBuffer[rcvBufferStart];
		rcvBufferStart++;
		if(rcvBufferStart == RCV_BUFFER_SIZE){
			rcvBufferStart = 0;
		}
		if(i <= 9)checksum = checksum + msg[i];
	}

	if(checksum == (msg[11] << 8 | msg[10])){
		// ACK Checksum OK
	}else{
		// ACK Checksum Fail
		lcd_clrscr();
		lcd_gotoxy(0, 0);
		lcd_puts("Greska");
	}
	rcvParameter = (uint32_t)msg[4] | (uint32_t)msg[5] << 8 | (uint32_t)msg[6] << 16 | (uint32_t)msg[7] << 24;
	rcvResponse = (uint16_t)msg[9] << 8 | (uint16_t)msg[8];

	if(rcvResponse == 0x0031){
		if(rcvParameter == 0x100A) return;
		if(rcvParameter == 0x1012){
			lcd_clrscr();
			lcd_gotoxy(0, 0);
			lcd_puts("Prst ne");
			
			lcd_gotoxy(0, 1);
			lcd_puts("pritiskuje");
			_delay_ms(1500);
		}
	}else if(rcvResponse == 0x0030){
		// ACK OK, continue
	}
}

void ledOn(){
	sendCommand(0x0012, 1);
	receiveAck();
}

void ledOff(){
	sendCommand(0x0012, 0);
	receiveAck();
}

void delete_all(){
	sendCommand(0x0041, 0);
	receiveAck();
	
	lcd_clrscr();
	lcd_gotoxy(0, 0);
	lcd_puts("Svi korisnici");
	lcd_gotoxy(0,1);
	lcd_puts("izbrisani");
	_delay_ms(1500);
}

int is_enrolled(int id){
	sendCommand(0x0021, id);
	receiveAck();
	
	if(rcvResponse == 0x0030){
		return 1;
	}else{
		return 0;
	}
}



// 0 if finger not on sensor
int isFingerPressing(){
	sendCommand(0x0026, 0);
	receiveAck();
	
	if(rcvParameter == 0){
		uint8_t temp = PIND & 0x20;
		if(temp == 0){  // ICPCK check
			return 1;
		}else{
			return 0;
		}
	}else{
		return 0;
	}
}

void enroll(){
	// find first free ID
	int id;
	for(id=0; id <= 2999; id++){
		if(is_enrolled(id) == 0) break;
	}
	
	if(id == 3000){
		lcd_clrscr();
		lcd_gotoxy(0, 0);
		lcd_puts("Greska");
		
		lcd_gotoxy(0, 1);
		lcd_puts("Baza je puna");
		_delay_ms(2000);
		return;
	}
	
	lcd_clrscr();
	lcd_gotoxy(0, 0);
	lcd_puts("Prislonite prst");
	lcd_gotoxy(0, 1);
	lcd_puts("1/3");
	
	while(isFingerPressing() == 0)_delay_ms(400);
	sendCommand(0x0022, id);    // Start enroll
	receiveAck();
	
	sendCommand(0x0060, 1);     // Capture finger
	receiveAck();
	if(rcvResponse == 0x0031 && rcvParameter == 0x1012)return;

	sendCommand(0x0023, 0);     // Enroll 1
	receiveAck();
	
	lcd_clrscr();
	lcd_gotoxy(0, 0);
	lcd_puts("Uklonite prst");
	
	while(isFingerPressing() == 1)_delay_ms(400);
	
	lcd_clrscr();
	lcd_gotoxy(0, 0);
	lcd_puts("Prislonite prst");
	lcd_gotoxy(0, 1);
	lcd_puts("2/3");
	
	while(isFingerPressing() == 0)_delay_ms(400);
	
	sendCommand(0x0060, 1);     // Capture finger
	receiveAck();
	if(rcvResponse == 0x0031 && rcvParameter == 0x1012)return;
	
	sendCommand(0x0024, 0);     // Enroll 2
	receiveAck();
	
	
	lcd_clrscr();
	lcd_gotoxy(0, 0);
	lcd_puts("Uklonite prst");
	
	while(isFingerPressing() == 1) _delay_ms(400);
	lcd_clrscr();
	lcd_gotoxy(0, 0);
	lcd_puts("Prislonite prst");
	lcd_gotoxy(0, 1);
	lcd_puts("3/3");

	while(isFingerPressing() == 0)_delay_ms(400);
	
	sendCommand(0x0060, 1);     // Capture finger
	receiveAck();
	if(rcvResponse == 0x0031 && rcvParameter == 0x1012)return;

	sendCommand(0x0025, 0);     // Enroll 3
	receiveAck();
	
	char buff[16];
	sprintf(buff, "Korisnik %d", id);
	lcd_clrscr();
	lcd_gotoxy(0, 0);
	lcd_puts(buff);
	lcd_gotoxy(0, 1);
	lcd_puts("dodan!");
	_delay_ms(1500);
}


int identify_fingerprint(int noMsg){
	lcd_clrscr();
	lcd_gotoxy(0, 0);
	lcd_puts("Prisloni prst");
	while(isFingerPressing() == 0) _delay_ms(400);

	sendCommand(0x0060, 1);     // Capture finger
	receiveAck();

	sendCommand(0x0051, 1);     // Identify fingerprint
	receiveAck();

	if((noMsg == 0) && (rcvResponse != 48)){
		lcd_clrscr();
		lcd_gotoxy(0, 0);
		lcd_puts("Korisnik nije");
		lcd_gotoxy(0, 1);
		lcd_puts("prepoznat");
	}else if(noMsg == 0){
		char buff[16];
		sprintf(buff, "Korisnik %lu", rcvParameter);
		lcd_clrscr();
		lcd_gotoxy(0, 0);
		lcd_puts(buff);
		lcd_gotoxy(0, 1);
		lcd_puts("je prepoznat");
	}
	
	if(noMsg == 0) _delay_ms(3000);
	
	if(rcvResponse != 0x0030) return -1;
	else return rcvParameter;
}

void delete_user(){
	int user = identify_fingerprint(1);
	if(user == -1){
		lcd_clrscr();
		lcd_gotoxy(0, 0);
		lcd_puts("Korisnik nije");
		
		lcd_gotoxy(0, 1);
		lcd_puts("prepoznat");
		_delay_ms(1500);
		return;
	}
	
	sendCommand(0x0040, user);
	receiveAck();
	
	char buff[16];
	sprintf(buff, "Korisnik %d", user);
	lcd_clrscr();
	lcd_gotoxy(0, 0);
	lcd_puts(buff);
	
	lcd_gotoxy(0, 1);
	lcd_puts("izbrisan");
	_delay_ms(1500);
}

char muscle_sensor(){
	ADCSRA |= (1 << ADSC);
	while (!(ADCSRA & (1 << ADIF)));
	_delay_ms(400);
	return ADC;
}

void init_fingerprint_scanner(){
	_delay_ms(100);
	sendCommand(0x0001, 0);
	_delay_ms(100);
	receiveAck();
}


void init_adc() {
	ADMUX = (1 << REFS0) | (1 << MUX2);
	ADCSRA = (1 << ADEN) | (1 << ADPS1) | (1 << ADPS0);
}


void play_game(){
	lcd_clrscr();
	init_adc();
	lcd_gotoxy(0,0);
	lcd_puts("Registriraj se");
	lcd_gotoxy(0,1);
	lcd_puts("da bi igrao");
	_delay_ms(5000);
	
	int p1_id = -1;
	int p2_id = -1;
	int p1_score = 0;
	int p2_score = 0;
	lcd_clrscr();
	lcd_puts("Igrac 1 id");
	while (p1_id == -1) {
		p1_id = identify_fingerprint(1);
	}
	lcd_clrscr();
	lcd_puts("Igrac 2 id");
	while (p2_id == -1) {
		p2_id = identify_fingerprint(1);
	}
	lcd_clrscr();
	lcd_puts("Igrac 1 igraj");
	_delay_ms(3000);
	while(p1_score == 0){
		p1_score = muscle_sensor();
	}
	
	lcd_clrscr();
	lcd_puts("Igrac 2 igraj");
	while(p2_score == 0){
		p2_score = muscle_sensor();
	}
	_delay_ms(3000);
	
	if (p1_score > p2_score) {
		lcd_puts("Igrac 1 pobjedio");
		} else {
		lcd_puts("Igrac 2 pobjedio");
	}
	_delay_ms(5000);
	return;
}




void message(){
	lcd_clrscr();
	lcd_gotoxy(0, 0);
	lcd_puts("Sustav");
	
	lcd_gotoxy(0, 1);
	lcd_puts("pokrenut");
}


void playGame(){
	lcd_clrscr();
	int p1_id = -1;
	int p2_id = -1;
	int p1_score = 0;
	int p2_score = 0;
	
	lcd_puts("Player 1 id");
	while (p1_id == -1) {
		p1_id = identify_fingerprint();		
	}
	
	lcd_puts("Player 2 id");
	while (p2_id == -1) {
		p2_id = identify_fingerprint();
	}
	
	lcd_puts("Player 1 play");
	//Muscle sensor
	//Ocitavati vrijednost mišiæa kroz period od 5-10 sekundi i spremiti najvišu vrijednost u p1_score
	
	lcd_puts("Player 2 play");
	//Ista stvar za p2_score
	
	if (p1_score > p2_score) {
		lcd_puts("Player 1 win");
	} else {
		lcd_puts("Player 2 win");
	}
	_delay_ms(5000);	
	return;	
}



int main(void)
{
	DDRB = 0xff;
	PORTB = 0x0f;
	
	PORTD = (1 << 6);
	DDRD = (1 << 4) | (1 << 6);

	TCCR1A = (1 << COM1B1) | (1 << WGM10);
	TCCR1B = (1 << WGM12) | (1 << CS11);
	OCR1B = 60;

	lcd_init(LCD_DISP_ON);
	lcd_clrscr();

	UBRRH = (unsigned char)(BAUDRATE>>8);
	UBRRL = (unsigned char)BAUDRATE;

	UCSRB = (1 << RXEN) | (1 << TXEN) | (1 << RXCIE);
	UCSRC = (1 << URSEL) | (1 << UCSZ1) | (1 << UCSZ0);

	sei();
	init_fingerprint_scanner();
	ledOn();
	
	while(1){
		if((PINB & 0x01) == 0){
			identify_fingerprint(0);
			//play_game();
			}else if((PINB & 0x02) == 0){
			enroll();
			}else if((PINB & 0x04) == 0){
			delete_user();
			}else if((PINB & 0x08) == 0){
			delete_all();
		}
		
		message();
		_delay_ms(1600);
	}

}
