/*
 * Project Master.c
 *
 * Created: 11/20/2018 1:31:46 PM
 * Author : Derek
 */ 

#define F_CPU 8000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <usart_ATmega1284.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <util/delay.h>
#include "io.c"

/*
*	Global Vars
*/
uint16_t tempVal;	//Global value for temperature
uint16_t smVal;		//Global value for soil moisture
char tempDisp[4];	//String use to display temperature
char smDisp1[4];	//String use to display soil moisture levels of pot 1
char smDisp2[4];	//String use to display soil moisture levels of pot 2
char smDisp3[4];	//String use to display soil moisture levels of pot 3
char smDisp4[4];	//String use to display soil moisture levels of pot 4

unsigned char workingOn = 0;


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

//Initializes ADC with 128 prescaler
void ADC_Init() {
	ADCSRA |= (1<<ADEN) | (1<<ADPS0) | (1<<ADPS1) | (1<<ADPS2);
	// ADEN: setting this bit enables analog-to-digital conversion.
	// ADPS0 ADPS1 ADPS3 prescaler to 128
}

//Reads in ADC value at pin number less than or equal to 7
uint16_t ADC_Read(unsigned char pinNum)
{
	pinNum = pinNum & 0x07;//Make sure its 7 or less
	
	//Set mux to input pin number
	ADMUX = (ADMUX & 0xF8) | pinNum;
	
	// Set single conversion bit
	ADCSRA |= (1<<ADSC);
	
	//wait for conversion to end.
	//ADSC bit of ADCSRA is set low when conversion is complete
	while(ADCSRA & (1<<ADSC));
	
	return ADC;
}

//Displays input from sensors and fan setting.
void displaySensors()
{
	//Display Temp
	LCD_ClearScreen();
	LCD_DisplayStringNC(1, "TMP");
	LCD_DisplayStringNC(17, tempDisp);
	LCD_Cursor((17+strlen(tempDisp)));
	LCD_WriteData('C');
	
	//Display Soil Moisture Percentage
	LCD_DisplayStringNC(5,"SM%");
	LCD_DisplayStringNC(9, smDisp1);
	LCD_DisplayStringNC(13, smDisp2);
	LCD_DisplayStringNC(25, smDisp3);
	LCD_DisplayStringNC(29, smDisp4);
	
	LCD_Cursor(32);
}

void displayQueueSM(struct Queue* queue)
{
	LCD_ClearScreen();
	LCD_DisplayString(1, "Current: ");
	LCD_Cursor(10);
	if(workingOn == 0){
		LCD_DisplayStringNC(10, "Home");
	}
	else{
		LCD_WriteData(workingOn + '0');
	}
	LCD_DisplayStringNC(17, "Ready: ");
	unsigned char cursor = 24;
	for(unsigned i = 0; i < queue->size; ++i)
	{
		char tmp = (char) queue->array[(queue->front + i)];
		LCD_Cursor(cursor);
		LCD_WriteData(tmp + '0');
		cursor = cursor + 2;
	}
}

//Reads in smVal and determines its percentage. The soil moisture sensor returns voltage VIN amount equal to VREF when no soil or in air.
//The equation that determins this is (Vin * 1024)/Vref. The percentages are every 5%.
const char * Set_SMPercentage(uint16_t val)
{
	if(val > 1000)
	{
		return "0%";
	}
	else if(val > 950)
	{
		return "5%";
	}
	else if(val > 900)
	{
		return "10%";
	}
	else if(val > 850)
	{
		return "15%";
	}
	else if(val > 800)
	{
		return "20%";
	}
	else if(val > 750)
	{
		return "25%";
	}
	else if(val > 700)
	{
		return "30%";
	}
	else if(val > 650)
	{
		return "35%";
	}
	else if(val > 600)
	{
		return "40%";
	}
	else if(val > 550)
	{
		return "45%";
	}
	else if(val > 500)
	{
		return "50%";
	}
	else if(val > 450)
	{
		return "55%";
	}
	else if(val > 400)
	{
		return "60%";
	}
	else if(val > 350)
	{
		return "65%";
	}
	else if(val > 300)
	{
		return "70%";
	}
	else if(val > 250)
	{
		return "75%";
	}
	else if(val > 200)
	{
		return "80%";
	}
	else if(val > 150)
	{
		return "85%";
	}
	else if(val > 100)
	{
		return "90%";
	}
	else if(val > 50)
	{
		return "95%";
	}
	else
	{
		return "100%";
	}
}

//Reads in temperature and soil moisture sensors on alternating states
enum Sensor_States{Sensor_Start, Temp_Read, SM1_Read, SM2_Read, SM3_Read, SM4_Read} Sensor_State;
void sensorSM(struct Queue* queue)
{
	switch(Sensor_State)
	{
		case Sensor_Start:
			tempVal = 0;
			tempDisp[0] = '0';
			smVal = 0;
			smDisp1[0] = '0';
			smDisp2[0] = '0';
			smDisp3[0] = '0';
			smDisp4[0] = '0';
			Sensor_State = SM1_Read;
			break;
		case Temp_Read:
			tempVal = (ADC_Read(0x00))/2;//Temp pin is on PA0
			itoa(tempVal, tempDisp, 10);
			Sensor_State = SM1_Read;
			break;
		case SM1_Read:
			smVal = ADC_Read(0x01);// SM1 Pin is on PA1
			strcpy(smDisp1, Set_SMPercentage(smVal));
			if(smVal < 950 && smVal > 700){
				enqueue(queue, 1);
			}
			Sensor_State = SM2_Read;
			break;
		case SM2_Read:
			smVal = ADC_Read(0x02);// SM2 Pin is on PA2
			strcpy(smDisp2, Set_SMPercentage(smVal));
			if(smVal < 950 && smVal > 700){
				enqueue(queue, 2);
			}
			Sensor_State = SM3_Read;
			break;
		case SM3_Read:
			smVal = ADC_Read(0x03);// SM2 Pin is on PA3
			strcpy(smDisp3, Set_SMPercentage(smVal));
			if(smVal < 950 && smVal > 700){
				enqueue(queue, 3);
			}
			Sensor_State = SM4_Read;
			break;
		case SM4_Read:
			smVal = ADC_Read(0x04);// SM2 Pin is on PA2
			strcpy(smDisp4, Set_SMPercentage(smVal));
			if(smVal < 950 && smVal > 700){
				enqueue(queue, 4);
			}
			Sensor_State = Temp_Read;
			break;
		default:
			tempVal = 0;
			tempDisp[0] = '0';
			smVal = 0;
			smDisp1[0] = '0';
			smDisp2[0] = '0';
			smDisp3[0] = '0';
			smDisp4[0] = '0';
			Sensor_State = Sensor_Start;
			break;
	}
}

enum buttonStates {buttonStart, buttonWait, buttonPressed} buttonState;
unsigned char dispOpt = 1;
void buttonSM(struct Queue* queue)
{
	unsigned char b1 = (PINB & 0x01);
	unsigned char b2 = (PINB & 0x02);
	unsigned char b3 = (PINB & 0x04);
	unsigned char b4 = (PINB & 0x08);
	unsigned char b5 = (PINB & 0x10);
	
	switch(buttonState)
	{
		case buttonStart:
			buttonState = buttonWait;
			break;
		case buttonWait:
			if(!b1 && !b2 && !b3 && !b4 && !b5)
			{
				buttonState = buttonWait;
			}
			if(isFull(queue))
			{
				break;
			}
			else if(!b1)
			{
				//Switch displays	
				dispOpt = (dispOpt == 1)? 0 : 1;
				buttonState = buttonPressed;
			}
			else if(!b2)
			{
				buttonState = buttonPressed;
				if(workingOn != 1){
					enqueue(queue, 1);
				}
			}
			else if(!b3)
			{
				buttonState = buttonPressed;
				if(workingOn != 2){
					enqueue(queue, 2);
				}
			}
			else if(!b4)
			{
				buttonState = buttonPressed;
				if(workingOn != 3){
					enqueue(queue, 3);
				}
			}
			else if(!b5)
			{
				buttonState = buttonPressed;
				if(workingOn != 4){
					enqueue(queue, 4);
				}
			}
			break;
		case buttonPressed:
			if(b1 && b2 && b3 && b4 && b5)
			{
				buttonState = buttonWait;
			}
			break;
		default:
			buttonState = buttonStart;
			break;
	}
}

void err(){
	LCD_DisplayString(1, "ERROR");
	while(1){;}
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
				
				if(USART_Receive(0) == 0xFF)//If master receives ready msg
				{
					tmpM = (char) dequeue(queue);
					
					USART_Send(tmpM, 0);
					workingOn = tmpM;
					masterState = masterWait;
					break;
				}
			}
			if(USART_HasReceived(0))
			{
				tmpM = USART_Receive(0);
				if(tmpM == 0x55)//If master receives home msg
				{
					LCD_DisplayString(1, "Going Home");
					workingOn = 0;
					while(1)
					{
						if(USART_HasReceived(0))
						{
							if(USART_Receive(0) == 0xFF)
							{
								break;
							}
							if(USART_Receive(0) == 0x0F)
							{
								err();
							}
						}
					}
				}
				else if(tmpM == 0x0F)//If master receives error msg
				{
					err();
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

int main(void)
{
	//Master Configs
	DDRA = 0x00; PORTA = 0x00; // ADC Sensors
	DDRB = 0x1F; PORTB = 0x1F; // Buttons PB0-PB3
	DDRC = 0xFF; PORTC = 0x00; // LCD Display PC0-PC7
	DDRD = 0xC0; PORTD = 0x00; // USART PD0 & PD1. LCD Display PD6 & PD7
	
	//Init Queue, LCD, USART, ADC
	struct Queue* q = createQueue(3);
	initUSART(0);
	ADC_Init();
	LCD_init();
	
	TimerSet(100);
	TimerOn();
	
    while (1) 
    {
		unsigned char sw1 = (PINA & 0x20);
		masterSM(q);
		
		if(!sw1){
			sensorSM(q);
		}
		buttonSM(q);	
		if(dispOpt){
			displayQueueSM(q);
		}
		else{
			displaySensors();
		}
		
		while(!TimerFlag);
		TimerFlag = 0;
    }
}