// Это прошивка для контролллера весов. Производит измерения и отправляет на принимающий контрллер



#include <math.h>
#include <SoftwareSerial.h>                                   // Библиотека програмной реализации обмена по UART-протоколу
#include "HX711.h"
#define DOUT  A0                        // Подключение HX711
#define CLK  A1                         // Подключение HX711
#define pin_read A3            // Напряжение питания
#include <EEPROM.h>     // Библиотека для работы с энергонезависимой памятью
#include <SPI.h>          // библиотека для работы с шиной SPI
#include "nRF24L01.h"     // библиотека радиомодуля
#include "RF24.h"         // ещё библиотека радиомодуля
#include <GyverPower.h>     // управление питанием Arduino


RF24 radio(10, 9);       // "создать" модуль на пинах 9 и 10
HX711 scale(DOUT, CLK);
float calibration_factor = -23690;               //Калибровочный фактор

byte address[][6] = {"1Node", "2Node", "3Node", "4Node", "5Node", "6Node"}; //возможные номера труб

float sclROMInit;
float sclROM, sclROM2=0;
bool flgInitScl = false;          //Флаг выбора программы измерений
bool flgSetup = true;

uint32_t myTimer1 = 0;   //Таймеры для loop

bool hasmsg = false;                                              // Флаг наличия сообщений к удалению

//**********Сброс значения памяти*************
float ROMinit(){
  EEPROM.put(10, 0.00);
  sclROMInit = 0.00;
  scale.tare();
  //Serial.println ("Весы и пямять сброшены!");
  return (0.00);
}
//--------------------------------------------------------------------

//********Измерение веса и запись в память************
float sclMEAS(){
  float diffScl;
  if (flgInitScl == true){
	diffScl =  fabs(sclROM) - scale.get_units(50);
	if (diffScl > 0.05 || diffScl < -0.05){
		sclROM = scale.get_units(50);
		EEPROM.put(10, sclROM);
		//Serial.println (String (diffScl) + " кг. Разница!");
		//Serial.print (String (sclROM) + " кг. Записано в память!");
		//Serial.println ("");
    }
    else{
      //Serial.println (String (diffScl) + " кг. Разница!");
	  //Serial.println (String (sclROM) + " кг. Вес!");
    }
  }
  else{
    diffScl = fabs(sclROM) - fabs(sclROMInit + scale.get_units(50));
    if (diffScl > 0.05 || diffScl < -0.05){
		sclROM = sclROMInit + scale.get_units(50);
		EEPROM.put(10, sclROM);
		//Serial.println (String (diffScl) + " кг. Разница!");
		//Serial.println (String (sclROM) + " кг. Сумма записана в память!");
		//Serial.println ("");
    }
    else{
		//Serial.println (String (diffScl) + " кг. Разница!");
		//Serial.println (String (sclROM) + " кг. Вес!");
    }
  } 
  return sclROM;
}
//-------------------------------------------------------------------------------------------

//*************Измерение напряжения на батареи***********************************
float uBat (){
  float k = 2*1.95;
  float voltage = k*4.5f/1024*analogRead(pin_read);
  return voltage;
 }
//----------------------------------------------------------------------------------------------


//***************Отправка данных**************************************************
void SendData(float sclROM){
  //-----Отправка веса-----
  int gotByte, ingotByte;
  int intscl=0, counter = 0, counter2 = 0;
  intscl = sclROM * 100;
  radio.powerUp();		//включение NRF
  delay(1000);
  while (counter < 50){
    //Serial.println(counter);
    //Serial.println(intscl);
    radio.write(&intscl, sizeof(intscl));
    if(!radio.available()){                     //если получаем пустой ответ
    }
	else{  
		if(radio.available()) {                      // если в ответе что-то есть
        radio.read( &ingotByte, 2 );                  // читаем
		//Serial.print("Ответ "); //Serial.println(ingotByte);
			if (intscl == ingotByte){
				counter = 50;
			}
		}
    }  
	delay(1000);
	counter++;
  }
  //--------Отправка напряжения-------
  while (counter2 < 50){
    int VluBat = (uBat()*100) + 20000;   //подготовка значения напряжения для отправки
	//Serial.println(counter2);
	//Serial.println(VluBat);
    radio.write(&VluBat, sizeof(VluBat));
    if(!radio.available()){                     //если получаем пустой ответ
    }
	else{  
		if(radio.available()) {                      // если в ответе что-то есть
			radio.read( &ingotByte, 2 );                  // читаем
			//Serial.println("Ответ "); //Serial.println(ingotByte);
			if (VluBat == ingotByte){
			counter2 = 50;
			}
		}
    }  
	delay(1000);
	counter2++;
  }
  radio.powerDown(); 		//отключение NRF
}
//--------------------------------------------------------------------------------------------

void setup() {
  Serial.begin(9600);                                         // Скорость обмена данными с компьютером
  //Serial.println("Start!");
  
  power.autoCalibrate(); // автоматическая калибровка таймера сна (чтобы спал точное время)
  
  //---Вход для измерения напряжения---
  pinMode(pin_read, INPUT);
    
  //---Настройка весов---
  pinMode(3, INPUT);                   //PIN подключения кнопки
  EEPROM.get(10, sclROMInit);               //Считывание данных с памяти  
  //Serial.print (String (sclROMInit) + " кг. Прочитано из памяти!");
  //Serial.println ("");
  scale.set_scale(calibration_factor);              //Установка колибровочного коэффициента
  scale.tare();                         //Сброс весов в 0
  
  //---Настройка модуля---
  radio.begin(); //активировать модуль
  radio.setAutoAck(1);         //режим подтверждения приёма, 1 вкл 0 выкл
  radio.setRetries(0, 15);    //(время между попыткой достучаться, число попыток)
  radio.enableAckPayload();    //разрешить отсылку данных в ответ на входящий сигнал
  radio.setPayloadSize(32);     //размер пакета, в байтах
  radio.openWritingPipe(address[1]);   //мы - труба 0, открываем канал для передачи данных
  radio.setChannel(0x65);  //выбираем канал (в котором нет шумов!)
  radio.setPALevel (RF24_PA_MAX); //уровень мощности передатчика. На выбор RF24_PA_MIN, RF24_PA_LOW, RF24_PA_HIGH, RF24_PA_MAX
  radio.setDataRate (RF24_1MBPS); //скорость обмена. На выбор RF24_2MBPS, RF24_1MBPS, RF24_250KBPS
  //должна быть одинакова на приёмнике и передатчике!
  //при самой низкой скорости имеем самую высокую чувствительность и дальность!!
  radio.powerUp(); //начать работу
  radio.stopListening();  //не слушаем радиоэфир, мы передатчик
}

void loop() {
	//Serial.println("Loop");
	if (flgSetup == false){
	delay(1000);
	}
	if (digitalRead(3) == HIGH){           //Если кнопка сброса нажата
		//Serial.println("Нажата кнопка сброса !");
		sclROM = ROMinit();
		flgInitScl = true;         //Устанавливаем Флаг выбора программы измерений
	}
	//Измерение веса
	sclROM = sclMEAS();         //Измерение веса
	if (millis() - myTimer1 >= 600000 || millis() < 600000) {   // таймер на 10 мин
		myTimer1 = millis();              // сброс таймера
		//Serial.println("10m");
		SendData(sclROM);
	}
	if (millis() > 600000){
		//Serial.println("Sleep!");
		flgSetup = false;
		power.sleepDelay(60000);  // спим 60 секунд 
	}
}