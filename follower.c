/*
 * 122A_ Project_Follower.c
 * Derek Sayler: dsayl001@ucr.edu
 * 
 * This is the USART follower code the handles stepping logic for the CNC smart garden.
 * Sends USART msg to master based on current state.
 * Ready: 0xFF
 * Wait: 0xF0
 * Setup: 0x55
 * Error: 0x0F
 *
 * I acknowledge all content contained herein, excluding template or example code,
 * is my own work
 */
#define F_CPU 8000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include <usart_ATmega1284.h>
#include "io.c"
#include "bit.h"

#define HomeDir 0
#define AltDir 1
#define StepGap 1248

unsigned char currentPos = 0;

volatile unsigned char TimerFlag = 0;

// Internal variables for mapping AVR's ISR to our cleaner TimerISR model.
unsigned long _avr_timer_M = 1;         // Start count from here, down to 0. Default 1 ms.
unsigned long _avr_timer_cntcurr = 0;   // Current internal count of 1ms ticks

void TimerOn()
{
	// AVR timer/counter controller register TCCR1
	TCCR1B = 0x0B;// bit3 = 0: CTC mode (clear timer on compare)
	// bit2bit1bit0=011: pre-scaler /64
	// 00001011: 0x0B
	// SO, 8 MHz clock or 8,000,000 /64 = 125,000 ticks/s
	// Thus, TCNT1 register will count at 125,000 ticks/s
	// AVR output compare register OCR1A.
	OCR1A = 125;    // Timer interrupt will be generated when TCNT1==OCR1A
	// We want a 1 ms tick. 0.001 s * 125,000 ticks/s = 125
	// So when TCNT1 register equals 125,
	// 1 ms has passed. Thus, we compare to 125.
	// AVR timer interrupt mask register
	TIMSK1 = 0x02; // bit1: OCIE1A -- enables compare match interrupt

	//Initialize avr counter
	TCNT1 = 0;

	_avr_timer_cntcurr = _avr_timer_M;
	// TimerISR will be called every _avr_timer_cntcurr milliseconds

	//Enable global interrupts
	sei(); // 0x80: 1000000
}

void TimerOff()
{
	TCCR1B = 0x00; // bit3bit1bit0=000: timer off
}

void TimerISR()
{
	TimerFlag = 1;
}

// In our approach, the C programmer does not touch this ISR, but rather TimerISR()
ISR(TIMER1_COMPA_vect)
{
	// CPU automatically calls when TCNT1 == OCR1 (every 1 ms per TimerOn settings)
	_avr_timer_cntcurr--;               // Count down to 0 rather than up to TOP
	if (_avr_timer_cntcurr == 0)
	{                                   // results in a more efficient compare
		TimerISR();                     // Call the ISR that the user uses
		_avr_timer_cntcurr = _avr_timer_M;
	}
}

// Set TimerISR() to tick every M ms
void TimerSet(unsigned long M)
{
	_avr_timer_M = M;
	_avr_timer_cntcurr = _avr_timer_M;
}

//Dir pin B0 (0x01)
void changeDir(unsigned char dir){
	dir = (dir & 0x01);
	PORTD = dir ? PORTD | 0x10 : PORTD & ~0x10;
	_delay_us(1.30);//Needs 650 ns for stabilization 
	return;
}

//Step Pin B1 (0x02)
void step(){
	PORTD |= 0x20;
	_delay_us(2.0); //Min delay of 2 microseconds for step pulse
	PORTD &= 0xDF;
	_delay_ms(10.0);
}

void stepBack(unsigned char choice){
	changeDir(choice);
	_delay_ms(10.0);
	for(unsigned char i = 0; i < 10; ++i)
	{
		step();
	}
}

void err(){
	while(1){
		if(USART_IsSendReady(0)) //Send error msg
		{
			USART_Send(0x0F, 0);
		}
	}
}

void goHome(){
	if(USART_IsSendReady(0))
	{
		USART_Send(0x55, 0);
	}
	
	changeDir(HomeDir);
	while(1){
		unsigned char ssHome = (PINA & 0x01);
		unsigned char ssAlt = (PINA & 0x02);
		if(!ssHome){
			_delay_ms(10.0);
			stepBack(AltDir);
			break;
		}
		if(!ssAlt)
		{
			err();
		}
		step();
	}
	currentPos = 0;
}

void stepperFunct(int stepNum){
	
	int i = 0;
	if(USART_IsSendReady(0))
	{
		USART_Send(0xF0, 0);
	}
	while(i < stepNum)
	{
		unsigned char ssHome = (PINA & 0x01);
		unsigned char ssAlt = (PINA & 0x02);
		
		if(!ssHome)
		{
			delay_ms(1000.0);
			err();
			while(1){;}
		}
		if(!ssAlt)
		{
			delay_ms(1000.0);
			err();
			while(1){;}
		}
		step();
		i = i + 1;
	}
	PORTC = 0x03;//Turn on water pump
	PORTB = 0x01;
	_delay_ms(750.0);
	PORTB = 0x03;
	_delay_ms(750.0);
	PORTB = 0x07;
	_delay_ms(750.0);
	PORTB = 0x0F;
	_delay_ms(750.0);
	PORTB = 0x00;
	PORTC = 0x01;//turn off pump but keep led grow light
}

void stepperLogic(unsigned char nextPos){
	int tmp = 0;
	if(currentPos == nextPos || nextPos > 4)
	{
		return;
	}
	if(nextPos > currentPos)
	{
		tmp = nextPos - currentPos;
		changeDir(AltDir);
		stepperFunct((tmp*StepGap));
	}
	if(nextPos < currentPos)
	{
		tmp = currentPos - nextPos;
		changeDir(HomeDir);
		stepperFunct((tmp*StepGap));
	}
	currentPos = nextPos;
}

unsigned char count = 0;
char tmpF = 0;
enum followerStates {followerStart, followerReady, followerWait} followerState;
void followerSM()
{
	switch(followerState)
	{
		case followerStart:
			cli();
			goHome();
			sei();
			PORTC = 0x01;//Turn on LED Grow Light
			count = 0;
			followerState = followerWait;
			break;
		case followerWait:
			if(USART_HasReceived(0))
			{
				count = 0;
				followerState = followerReady;
			}
			else if(USART_IsSendReady(0))//Tell master ready for next pot to water
			{
				USART_Send(0xFF, 0);
				count = count + 1;
				followerState = followerWait;
			}
			if(count == 10 && currentPos != 0)//If none come in 5 seconds go home and wait
			{
				cli();
				goHome();
				sei();
			}
			break;
		case followerReady:
			tmpF = USART_Receive(0); //Read in pot position and water
			//CNC logic
			cli();
			stepperLogic(tmpF);
			sei();
			followerState = followerWait;
			break;
		default:
			followerState = followerStart;
			break;
	}
	
}

int main(void)
{
	// TODO config ports
	DDRA = 0x00; PORTA = 0x00; // Stop Switches: PA0, PA1
	DDRB = 0x0F; PORTB = 0x00; // Watering LEDS: PB0 - PB3
	DDRD = 0x30; PORTD = 0x00; // UASRT: PD0, PD1. Stepper Motor: Direction Choice PD4, Step PD5
	DDRC = 0x03; PORTC = 0x00; // Relay Controls: LED Grow Light PC0, Water Pump PC1
	
	TimerSet(1000);
	TimerOn();
	
	initUSART(0);
	
	while(1){
		followerSM();
		
		while(!TimerFlag);
		TimerFlag = 0;
	}
	
	return 0;
}