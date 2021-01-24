/*
 * attiny406.c
 *
 * Created: 9/22/2020 5:06:26 PM
 * Author : jtperrotta
 */ 

#define F_CPU 20000000UL // ???? 
#include <avr/io.h>
#include <stdbool.h>
#include <stdio.h>

#include <util/delay.h>
#include <avr/interrupt.h>

volatile uint16_t _tx_delay = 0;

void init_timer(){
	TCA0.SINGLE.CTRLA = TCA_SINGLE_ENABLE_bm;
	TCA0.SINGLE.CTRLB = TCA_SINGLE_CMP2EN_bm | TCA_SINGLE_WGMODE1_bm | TCA_SINGLE_WGMODE0_bm;
	TCA0.SINGLE.CTRLC = TCA_SINGLE_CMP2OV_bm;

	TCA0.SINGLE.CMP2 = 128;
	TCA0.SINGLE.PER = 0xFF; // set top to 255
}


void initSerial(){
	_tx_delay = 0;
	uint16_t bit_delay = (F_CPU / 9600) / 4;
	if (bit_delay > 15 / 4)
		_tx_delay = bit_delay - (15 / 4);
	else
		_tx_delay = 1;
}
uint8_t readSerial(){
	cli();
	uint8_t ret = 0;
	_delay_loop_2(_tx_delay>>1);  
	_delay_loop_2(_tx_delay);
	for (uint8_t w = 0; w < 8;w++) {
		ret >>= 1;
		ret |= (PORTB.IN & (1<<5))?128:0;
		_delay_loop_2(_tx_delay);
	}
	sei();
	return ret;
}
void Send (uint8_t b) {
	uint8_t mask = 1<<4;
	uint8_t imask = ~mask;
	cli();
	PORTB.OUT &= imask;
	_delay_loop_2(_tx_delay);
	for (uint8_t i = 8; i > 0; --i) {
		if (b & 1)
		PORTB.OUT |= mask;
		else
		PORTB.OUT &= imask;
		_delay_loop_2(_tx_delay);
		b >>= 1;
	}
	PORTB.OUT |= mask;
	_delay_loop_2(_tx_delay);

	sei();
}


void setDiv(uint8_t val){
	TCA0.SINGLE.CTRLA = ((val & 7) << 1) | 1;
	TCA0.SINGLE.CTRLESET = (1 << 2); //reset timmer
}
void setFreq(uint16_t val){
	TCA0.SINGLE.PER = val;
	TCA0.SINGLE.CTRLESET = (1 << 2); //reset timmer
}

void setPWM(uint16_t val){
	TCA0.SINGLE.CMP2 = val;
	TCA0.SINGLE.CTRLESET = (1 << 2); //reset timmer
}
bool dos = true;


uint8_t  recv[100] = {0};


int8_t pollSerial(){
	uint8_t b = 0;
	while(1){
		if(b > 99){
			 return -1;
		}
		while(dos);
		recv[b] = readSerial();
		Send(recv[b]);
		if(recv[b] == '\0')
			break;
		if(recv[b] == '\n' || recv[b] == '\r'){
			recv[b+1] = '\0';
			break;
		}
		b++;
	}
	return b;
}

void str(char* chr){
	for(uint8_t j = 0; chr[j] != 0;j++)
		Send(chr[j]);
}
char *convert(unsigned long int num, int base)
{
	static char Representation[]= "0123456789ABCDEF";
	static char buffer[50];
	char *ptr;
	
	ptr = &buffer[49];
	*ptr = '\0';
	
	do
	{
		*--ptr = Representation[num%base];
		num /= base;
	}while(num != 0);
	
	return(ptr);
}

void print(char* format,...)
{
	char *traverse;
	unsigned long int i;
	char *s;
	
	//Module 1: Initializing Myprintf's arguments
	va_list arg;
	va_start(arg, format);
	
	for(traverse = format; *traverse != '\0'; traverse++)
	{
		while( *traverse != '%' )
		{
			Send(*traverse);
			traverse++;
			if(*traverse == 0){
				va_end(arg);
				return;
			}
		}
		traverse++;
		
		switch(*traverse)
		{
			case 'c' : i = va_arg(arg,int);		//Fetch char argument
			Send(i);
			break;
			
			case 'd' : i = va_arg(arg,int); 		//Fetch Decimal/Integer argument
			if(i<0)
			{
				i = -i;
				Send('-');
			}
			str(convert(i,10));
			break;
			case 'u' : i = va_arg(arg,unsigned long int); 		//Fetch Decimal/Integer argument
			if(i<0)
			{
				i = -i;
				Send('-');
			}
			str(convert(i,10));
			break;
			case 'o': i = va_arg(arg,long unsigned int); //Fetch Octal representation
			str(convert(i,8));
			break;
			
			case 's': s = va_arg(arg,char *); 		//Fetch string
			str(s);
			break;
			
			case 'x': i = va_arg(arg,long unsigned int); //Fetch Hexadecimal representation
			str(convert(i,16));
			break;
			default:
			Send('%');

		}
	}
	
	//Module 3: Closing argument list to necessary clean-up
	va_end(arg);
}


int main(void)
{
	CPU_CCP = 0xD8;
	CLKCTRL_MCLKCTRLA = CLKCTRL_CLKSEL_OSC20M_gc;
	CPU_CCP = 0xD8;
	CLKCTRL_MCLKCTRLB = 0;
	PORTB.DIR = 1 << 4 | 1 << 2;
	PORTB.OUT = 1 <<4;
	_delay_loop_2(0);
	_delay_loop_2(0);
	_delay_loop_2(0);

	init_timer();
	initSerial();
	PORTB.INTFLAGS = (1<<5); //enable int
	PORTB.PIN5CTRL = 11; //falling and pullup
	sei();
    while (1) 
    {
		print("->");
		uint8_t pwm = 50;
		uint32_t  freq = 0;
		pollSerial();
		if(sscanf((const char*)recv,"%lu %hhu",&freq,&pwm) < 1)
			if(sscanf((const char*)recv,"%lx %hhx",&freq,&pwm) < 1){
				print("bruh error you moron the syntax is 2 8 bit hex or decimal numbers that repersent the top and the and pwm\n\r");
				continue;
			}
		if(freq > F_CPU/20){
			print("\n\rmax freq: %u HZ\n\r",F_CPU/20);
			continue;
		}
		uint32_t top = F_CPU/freq;
		int i = 0;
		while(top > UINT16_MAX){
			top >>= 1;
			i++;
		}
		setDiv(i);
		uint16_t pwmm = (((uint32_t) top)*((uint32_t)pwm)/100);
		print("\n\r+--------+\n\rfreq: %u HZ\n\rpwm: %d% \n\r+--------+\n\r",freq,pwm);
		setFreq(top);
		setPWM(pwmm);
    }
}
ISR(PORTB_PORT_vect){
	dos = PORTB.IN & (1<<5); // fuster clucking on both edges even when set to neg
}

