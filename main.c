/*
 * GccApplication2.c
 *
 * Created: 11/11/2018 10:58:04 AM
 * Author : Derek
 */ 

#define F_CPU 8000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <usart_ATmega1284.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <util/delay.h>
#include "io.c"

// A structure to represent a queue
struct Queue
{
	int front, rear, size;
	unsigned capacity;
	int* array;
};

// function to create a queue of given capacity.
// It initializes size of queue as 0
struct Queue* createQueue(unsigned capacity)
{
	struct Queue* queue = (struct Queue*) malloc(sizeof(struct Queue));
	queue->capacity = capacity;
	queue->front = queue->size = 0;
	queue->rear = capacity - 1;  // This is important, see the enqueue
	queue->array = (int*) malloc(queue->capacity * sizeof(int));
	return queue;
}

// Queue is full when size becomes equal to the capacity
int isFull(struct Queue* queue)
{  return (queue->size == queue->capacity);  }

// Queue is empty when size is 0
int isEmpty(struct Queue* queue)
{  return (queue->size == 0); }

int inQueue(struct Queue* queue, int n)
{
	for(unsigned char i = 0; i < queue->size; ++i)
	{
		if(queue->array[i] == n)
		{
			return 1;
		}
	}
	return 0;
}

// Function to add an item to the queue.
// It changes rear and size
void enqueue(struct Queue* queue, int item)
{
	if (isFull(queue) || inQueue(queue, item))
		return;
	queue->rear = (queue->rear + 1)%queue->capacity;
	queue->array[queue->rear] = item;
	queue->size = queue->size + 1;
}

// Function to remove an item from queue.
// It changes front and size
int dequeue(struct Queue* queue)
{
	if (isEmpty(queue))
		return 0;
	int item = queue->array[queue->front];
	queue->front = (queue->front + 1)%queue->capacity;
	queue->size = queue->size - 1;
	return item;
}

// Function to get front of queue
int front(struct Queue* queue)
{
	if (isEmpty(queue))
		return INT_MIN;
	return queue->array[queue->front];
}

// Function to get rear of queue
int rear(struct Queue* queue)
{
	if (isEmpty(queue))
		return INT_MIN;
	return queue->array[queue->rear];
}

void displayQueue(struct Queue* queue)
{
	LCD_ClearScreen();
	LCD_DisplayString(1, "Enqueued");
	unsigned char cursor = 17;
	for(unsigned i = 0; i < queue->size; ++i)
	{
		char tmp = (char) queue->array[(queue->front + i)];
		LCD_Cursor(cursor);
		LCD_WriteData(tmp + '0');
		cursor = cursor + 2;
	}
}

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

//Dir pin C0 (0x01)
void changeDir(unsigned char dir){
	dir = (dir & 0x01);
	PORTC = dir ? PORTC | 0x01 : PORTC & ~0x01;
	_delay_us(1.30);//Needs 650 ns for stabilization
	return;
}

//Step Pin C1 (0x02)
void step(){
	PORTC |= 0x02;
	_delay_us(2.0); //Min delay of 2 microseconds for step pulse
	PORTC &= 0xFD;
	_delay_us(2.0);
}

enum buttonStates {buttonStart, buttonWait, buttonPressed} buttonState;
void buttonSM(struct Queue* queue)
{
	unsigned char b1 = (PINB & 0x01);
	unsigned char b2 = (PINB & 0x02);
	unsigned char b3 = (PINB & 0x04);
	
	
	switch(buttonState)
	{
		case buttonStart:
			buttonState = buttonWait;
			break;
		case buttonWait:
			if(!b1 && !b2 && !b3)
			{
				buttonState = buttonWait;
			}
			if(isFull(queue))
			{
				break;
			}
			else if(!b1)
			{
				buttonState = buttonPressed;
				enqueue(queue, 1);
			}
			else if(!b2)
			{
				buttonState = buttonPressed;
				enqueue(queue, 2);
			}
			else if(!b3)
			{
				buttonState = buttonPressed;
				enqueue(queue, 3);
			}
			break;
		case buttonPressed:
			if(b1 && b2 && b3)
			{
				buttonState = buttonWait;
			}
			break;
		default:
			buttonState = buttonStart;
			break;
	}
}
enum masterStates {masterStart, masterSend, masterWait} masterState;
unsigned char bit = 0;
char tmpM = 0;
void masterSM(struct Queue* queue)
{
	switch(masterState)
	{
		case masterStart:
			masterState = masterSend;
			break;
		case masterSend:
			if(USART_IsSendReady(0) && !(isEmpty(queue)) && USART_HasReceived(0))
			{
				if(USART_Receive(0) == 0xFF)
				{
					tmpM = (char) dequeue(queue);
					USART_Send(tmpM, 0);
					masterState = masterWait;
					break;
				}
			}
			masterState = masterSend;
			break;
		case masterWait:
			if(USART_HasTransmitted(0))
			{
				masterState = masterSend;
				break;
			}
			masterState = masterWait;
			break;
		default:
			masterState = masterStart;
			break;
	}
}

unsigned char choice = 1;
char tmpF = 0;
enum followerStates {followerStart, followerReady, followerWait} followerState;
void followerSM()
{	
	switch(followerState)
	{
		case followerStart:
			choice = 1;
			followerState = followerWait;
			break;
		case followerWait:
			if(USART_HasReceived(0))
			{
				followerState = followerReady;
			}
			else if(USART_IsSendReady(0))
			{
				USART_Send(0xFF, 0);		
				followerState = followerWait;
			}
			break;
		case followerReady:
			tmpF = USART_Receive(0);
			//TODO: CNC logic
			if(tmpF == 1)
			{
				PORTB = 0x01;
			}
			else if(tmpF == 2)
			{
				PORTB = 0x02;
			}
			else if(tmpF == 3)
			{
				PORTB = 0x04;
			}
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
	
	//Master Configs
	//DDRB = 0x07; PORTB = 0x07;
	//DDRC = 0xFF; PORTC = 0x00;
	//DDRD = 0xC0; PORTD = 0x00;
	
	//Follower Configs
	DDRA = 0x00; PORTA = 0x00;
	DDRB = 0x07; PORTB = 0x00;
	DDRC = 0x03; PORTC = 0x00;
	DDRD = 0x00; PORTD = 0x00;
    
    masterState = masterStart;
	followerState = followerStart;
	buttonState = buttonStart;
    
    // Set the timer and turn it on
    TimerSet(100);
    TimerOn();
	
	//Init Queue
	//struct Queue* q = createQueue(3);
	initUSART(0);
	LCD_init();
	LCD_DisplayString(1, "Enqueued");
	
	
	//enqueue(q, 1);
	//enqueue(q, 3);
	//enqueue(q, 1);
	//enqueue(q, 2);
	
	unsigned char count = 0;
	
    while(1)
    {
		unsigned char ss1 = (PINA & 0x01);
		//buttonSM(q);
		if(count >= 10)
		{
			//displayQueue(q);
			//masterSM(q);
			followerSM();
			count = 0;
			if(ss1)
			{
				step();
			}
		}
		++count;
	    while(!TimerFlag);
	    TimerFlag = 0;
    }
    
    return 0;
}

