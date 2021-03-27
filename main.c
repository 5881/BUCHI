//контроллер роторного испарителя 0.9
//В этой программе зашкаливоющее количество дефениций
#define F_CPU 11059200UL
#include <avr/io.h>
#include <avr/interrupt.h>
//#include <math.h>
#include <stdio.h>
#include <util/delay.h>
//Настройка UART
#define UART_SPEED 115200
#define UBBR_VALUE (F_CPU/UART_SPEED/16)-1
//Конфигурация дисплея
#define ST PC1
#define DS PC0
#define SH PC2
#define PORT_74HC545 PORTC 
//Расположение кнопок на панели	
//	PB0  PB1
//	         PD7
//	PD5  PD6
//Назначение кнопок
#define BUT1 !(PINB&(1<<PB0))
#define BUT2 !(PINB&(1<<PB1))
#define BUT3 !(PIND&(1<<PD7))
#define BUT4 !(PIND&(1<<PD6))
#define BUT5 !(PIND&(1<<PD5))
#define PUMP_RUN PORTC|=(1<<PC3)
#define PUMP_STOP PORTC&=~(1<<PC3)
#define CLAPAN_OPEN PORTC|=(1<<PC4)
#define CLAPAN_CLOSE PORTC&=~(1<<PC4)
//#define CLAPAN_STATE PORTC&(1<<PC4)
//#define PUMP_STATE PORTC&(1<<PC3)
#define HISTERESIS 10
int max_presure=740;
unsigned char count=0;
unsigned char symbol[11]={0xfa,0x82,0xb9,0xab,0xc3,\
    0x6b,0x7b,0xa2,0xfb,0xeb,0x00};//массив цифр
unsigned char out[4]={0xfa,0x82,0xb9,0xab};//отображаемый массив
unsigned char mode=0x00;
void usart_init(void){
	UBRRH=UBBR_VALUE>>8;
	UBRRL=UBBR_VALUE&0xff;
	//настройка конфигурации
	UCSRC|=(1<<URSEL)|(1<<UCSZ1)|(1<<UCSZ0);//8N1
	UCSRB|=(1<<RXEN)|(1<<TXEN);//включаем usart
	};

void put_char(unsigned char data,FILE *stream){
	while(!(UCSRA&(1<<5)));//ждём готовности
	UDR=data;//отправляем
	//return 0;
	};
	
static unsigned char get_char(FILE *stream){
	while(!(UCSRA&(1<<7)));
	return UDR;
	};

static FILE mystdout=\
	    FDEV_SETUP_STREAM(put_char,get_char,_FDEV_SETUP_RW);
	    
void timer0_init(){
    //тактовая частота FCPU/8
    TCCR0|=(1<<CS01);
    //Прерывание по переполнению
    TIMSK|=(1<<TOIE0);
    }
//Индикация динамическая на прерываниях, очень удачно вышло
//Для расширения порта использована 74HC545
 ISR(TIMER0_OVF_vect){
	static unsigned char dig=0;//отображаемая цифра
	
	unsigned char temp=mode|0x0F;//на всяк случай сброс катодов
	temp&=~(1<<(3-dig));
	
	for(unsigned char i=0;i<8;i++){//Засылаем байт mode в 74HC545
		if(temp&(1<<7)) PORT_74HC545|=(1<<DS);//Линия данных
		    else PORT_74HC545&=~(1<<DS);
		PORT_74HC545|=(1<<SH);//Линия такта
		temp<<=1;//Переходим к следующему биту
		PORT_74HC545&=~(1<<SH);
		}
		
	temp=out[dig];//считываем значение из массива
	for(unsigned char i=0;i<8;i++){//Засылаем символа в 74HC545
		if(temp&(1<<7)) PORT_74HC545|=(1<<DS);//Линия данных
		    else PORT_74HC545&=~(1<<DS);
		PORT_74HC545|=(1<<SH);//Линия такта
		temp<<=1;//Переходим к следующему биту
		PORT_74HC545&=~(1<<SH);
		}
	
	PORT_74HC545|=(1<<ST);//Выводим значение засланное в 74HC545
	PORT_74HC545&=~(1<<ST);
	dig++;//Переходим к следующей
	if(dig>3) dig=0;
    }
    
void adc_init(){
    //настройка ацп
    // напряжение сравнения внутренний источник 2,56 в, вход adc7
    ADMUX|=(1<<REFS0)|(1<<REFS1)|(1<<MUX2)|(1<<MUX1)|(1<<MUX0);
    //Частота fcpu/128, включение компаратора
    ADCSRA|=(1<<ADEN)|(1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0);
    }

int mesure(){//Замер с компенсацией шумов за счёт усреднения
    unsigned int temp=0;
    for(unsigned char i=0;i<64;i++){//набираем статистику
	ADCSRA|=(1<<ADSC); //запуск преобразования
	while(!(ADCSRA&(1<<ADIF)));//Ждём окончание преобразования
	ADCSRA|=(1<<ADIF);//сбрасываем флаг преобразования
	temp+=ADCW;
	}
    return (int)(temp>>6);
    }
int correct(unsigned int m){//перевод значений adc в torr
	unsigned int p=m-200;
	p*=1.47;
	return p;
	}

void indicate(unsigned int data){
    //unsigned int temp;
    unsigned char m=1, temp_out[4];
    //считываем значение
    //temp=mesure();
    //раскладываем значение на цыфры и переводим в символы
    for(unsigned char i=0;i<4;i++){
	temp_out[i]=symbol[data%10];
	if(i!=3) data/=10;
	};
    //избавляемся от ведущих нулей
    for(unsigned char i=3;i>0;i--){
    if(temp_out[i]==0xfa && m) temp_out[i]=0;
	else m=0;
	}
    //переписываем значения в массив вывода
    cli();
    for(unsigned char i=0;i<4;i++) out[i]=temp_out[i];
    sei();
    }
void button_scan(){
//Расположение кнопок на панели	
//	BUT1  BUT2
//	           BUT3
//	BUT5  BUT4
//Назначение кнопок
//  PB0 - RUN
	if(BUT1){
		mode|=(1<<6);//режим установки давления
		if (max_presure<800) max_presure+=1;
		if (count<13) count+=1;
		return;
		}
	if(BUT5){
		mode|=(1<<6);
		if(max_presure) max_presure-=1;
		if(count<13) count+=1;
		return;
		}
	if(BUT2){
		//mode=(1<<4);
		//RUN pump
		//cloce
		max_presure=0;
		return;
		}
	if(BUT3){
		//mode|=(1<<7);
		max_presure=740;
		PUMP_STOP; mode&=~(1<<5);
		CLAPAN_OPEN; mode|=1<<4;
		//stop pump
		//open
		return;
		}
	if(mode&(1<<6)){_delay_ms(500); mode&=~(1<<6); count=0;}	
	//mode|=(1<<5); //если ничего не нажато то стандартный режим.
	}

int main(void){
	DDRD=0;//Настройка портов
	DDRC=0xff;
	DDRB=0;
	PORTD=0b11100000;//тут висят кнопки
	PORTB=0b11;//тут висят кнопки
	PORTC=0;
	int presure;//пришлось добавить знак чтобы избежать 
	//проблем вблизи нуля
	usart_init();//Настройка UART
	timer0_init();//Настройка таймера0
	adc_init();//Настройка АЦП
	stdin=stdout=&mystdout;//Поток ввода-вывода
	printf("Shaman's pump controller\r\n");//отладочное сообщение
	sei();//разрешение прерываний
	while(1){
		//Насос и клапан работают независимо от режима отображения!
	    presure=mesure();//Замеряем текущее давление
	    presure=correct(presure);//делаем поправку(см ман датчика)
	    //если давление больше заданного, надо закрыть клапан
	    //если он открыт конечно.
	    if (presure>max_presure)
			{CLAPAN_CLOSE; mode&=~(1<<4);}
		//Если давление сильно больше заданного (больше гистерезиса)
		//надо включить насос
	    if (presure>(max_presure+HISTERESIS) && max_presure<730)
			{PUMP_RUN; mode|=(1<<5);}
		//Если давление меньше заданного, надо выключить насос
		//конечно если он включен
		if (presure<max_presure)
			{PUMP_STOP; mode&=~(1<<5);}
		//Если давление сильно меньше заданного, надо открыть клапан
		//Если он был закрыт.
		if (presure<(max_presure-HISTERESIS))
			{CLAPAN_OPEN; mode|=1<<4;}
	    button_scan();
	    if(mode&(1<<6)){
			//если делается настройка max_presure то отображаем 
			//задаваемое значение
			indicate(max_presure);
			//регулировка скорости изменения max_presure
			//сначало медленно, потом быстро
			if(count<13) _delay_ms(170); else _delay_ms(15);
			continue;
			}
		indicate(presure);
		_delay_ms(200);
			
	    };
	return 0;
    }
