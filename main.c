/*
 * main.c
 *
 * Created: 6/1/2018
 * Author : Derek A Sayler
 *
 * Prototype Master uC code for Smart Garden 2.0
 * Reads in sensor data and determins which pot needs watering
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include "io.c"
#include <string.h>

#define Temp_Pin 0x00
#define SM_Pin 0x01

//--Global Vars--------------------------------
unsigned long taskPeriod = 250;
	
uint16_t tempVal; //Global value for temperature
uint16_t smVal; //Global value for soil moisture
uint16_t tmpVal;

char tempDisp[4];//String use to display temperature
char smDisp[4];//String use to display soil moisture levels
unsigned char servoDisp;
//---------------------------------------------

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
enum Display_States{Disp_Start, Disp_Write} Display_State;
void TickFct_Display()
{
	switch (Display_State)
	{
		case Disp_Start:
			LCD_DisplayStringNC(1, "Moisture:");
			LCD_DisplayStringNC(17,"Temp:");
			Display_State = Disp_Write;
			break;
		case Disp_Write:
			LCD_DisplayStringNC(10, smDisp);
			LCD_Cursor((10+strlen(smDisp)));
			LCD_WriteData(' ');
			LCD_DisplayStringNC(22, tempDisp);
			LCD_Cursor((22+strlen(tempDisp)));
			LCD_WriteData('C');
			LCD_Cursor(16);
			Display_State = Disp_Write;
			break;
		default:
			Display_State = Disp_Start;
			break;
	}
}

//Reads in smVal and determines its percentage. The soil moisture sensor returns voltage VIN amount equal to VREF when no soil or in air. 
//The equation that determins this is (Vin * 1024)/Vref. The percentages are every 5%.
void Set_SMPercentage() 
{
	if(smVal > 1000)
	{
		strcpy(smDisp, "0%");
	}
	else if(smVal > 950)
	{
		strcpy(smDisp, "5%");
	}
	else if(smVal > 900)
	{
		strcpy(smDisp, "10%");
	}
	else if(smVal > 850)
	{
		strcpy(smDisp, "15%");
	}
	else if(smVal > 800)
	{
		strcpy(smDisp, "20%");
	}
	else if(smVal > 750)
	{
		strcpy(smDisp, "25%");
	}
	else if(smVal > 700)
	{
		strcpy(smDisp, "30%");
	}
	else if(smVal > 650)
	{
		strcpy(smDisp, "35%");
	}
	else if(smVal > 600)
	{
		strcpy(smDisp, "40%");
	}
	else if(smVal > 550)
	{
		strcpy(smDisp, "45%");
	}
	else if(smVal > 500)
	{
		strcpy(smDisp, "50%");
	}
	else if(smVal > 450)
	{
		strcpy(smDisp, "55%");
	}
	else if(smVal > 400)
	{
		strcpy(smDisp, "60%");
	}
	else if(smVal > 350)
	{
		strcpy(smDisp, "65%");
	}
	else if(smVal > 300)
	{
		strcpy(smDisp, "70%");
	}
	else if(smVal > 250)
	{
		strcpy(smDisp, "75%");
	}
	else if(smVal > 200)
	{
		strcpy(smDisp, "80%");
	}
	else if(smVal > 150)
	{
		strcpy(smDisp, "85%");
	}
	else if(smVal > 100)
	{
		strcpy(smDisp, "90%");
	}
	else if(smVal > 50)
	{
		strcpy(smDisp, "95%");
	}
	else
	{
		strcpy(smDisp, "100%");
	}
}

//Reads in temperature and soil moisture sensors on alternating states
enum Sensor_States{Sensor_Start, Temp_Read, SM_Read} Sensor_State;
unsigned char i = 1;
void TickFct_Sensor()
{	
	switch(Sensor_State)
	{
		case Sensor_Start:
			tempVal = 0;
			tempDisp[0] = '0';
			smVal = 0;
			smDisp[0] = '0';
			i = 1;
			Sensor_State = SM_Read;
			break;
		case Temp_Read:
			tempVal = (ADC_Read(0x00))/2;//Temp pin is on PA0
			itoa(tempVal, tempDisp, 10);
			Sensor_State =SM_Read;
			break;
		case SM_Read:
			smVal = ADC_Read(0x01);//Temp Pin is on PA1
			Set_SMPercentage();
			if(smVal > 960 && smVal < 700){
				//if moisture level is between 5% and 30% water
				//TODO: Send which pot needs watering
			}
			Sensor_State = Temp_Read;
			break;
		default:
			smVal = 0;
			smDisp[0] = '0';
			tempVal = 0;
			tempDisp[0] = '0';
			Sensor_State = Sensor_Start;
	}
}

//Function that initializes states and microcontroller functionalities
void Tasks_Init()
{	
	Display_State = Disp_Start;
	Sensor_State = Sensor_Start;
	
	LCD_init();//Initializes LCD settings
	ADC_Init();//Initializes ADC settings
	TimerOn();//Initializes Timer Settings
	TimerSet(taskPeriod);//Set Period
}

int main(void)
{
	DDRA = 0x00; PORTA = 0x00;//Inputs for Soil Moisture(PA1) and Temp Sensors(PA0)
	DDRD = 0xC0; PORTD = 0x00;//LCD output PB0 & PB1 and Servo PWM at PB6
	DDRC = 0xFF; PORTC = 0x00;//LCD outputs PC0 - PC7
	//Initialize 
	unsigned char i = 0;
	Tasks_Init();
	
    while (1) 
	{
		TickFct_Sensor();
		if(i>=2)
		{
			TickFct_Display();
			i = 0;
		}
		++i;
		while(!TimerFlag){}
		TimerFlag = 0;
	}
}