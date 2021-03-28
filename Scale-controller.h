// Это прошивка для контролллера весов. Производит измерения и отправляет на принимающий контрллер

#include <math.h>
#include <SoftwareSerial.h>                                   // Библиотека програмной реализации обмена по UART-протоколу
#include "HX711.h"
#include <EEPROM.h>     // Библиотека для работы с энергонезависимой памятью
#include <SPI.h>          // библиотека для работы с шиной SPI
#include "nRF24L01.h"     // библиотека радиомодуля
#include "RF24.h"         // ещё библиотека радиомодуля
#include <GyverPower.h>     // управление питанием Arduino
#include "DHT.h"    //Бибилотека для работы с DHT11

#define DHT_PIN 4 
#define DHT_TYPE DHT11
#define HX711_DOUT_PIN  A0                        // Подключение HX711
#define HX711_CLK_PIN  A1                         // Подключение HX711
#define PIN_BATTARY A3            // Напряжение питания
#define NRF24_CSN_PIN 9
#define NRF24_CE_PIN 10

//*************Настройка DHT11***************
DHT SensorDHT(DHT_PIN, DHT_TYPE);
RF24 NRF(NRF24_CE_PIN, NRF24_CSN_PIN);       // "создать" модуль на пинах 9 и 10
HX711 Scale(HX711_DOUT_PIN, HX711_CLK_PIN);
const float CALIBRATION_FACTOR = -23690;               //Калибровочный фактор

byte address[][6] = {"1Node", "2Node", "3Node", "4Node", "5Node", "6Node"}; //возможные номера труб

float weightRAMInit;
bool flgInitScale = false;          //Флаг выбора программы измерений
bool flgSetup = true;

uint32_t myTimer1 = 0;   //Таймеры для loop

struct  structData {
  float weight;
  float currentVoltage;
  float temp;
  float humidity;
  int count;
} data;

//**********Сброс значения памяти*************
void ROMinit(){
  EEPROM.put(10, 0.00);
  weightRAMInit = 0.00;
  Scale.tare();
  //Serial.println ("Весы и пямять сброшены!");
  data.weight = 0;
}
//--------------------------------------------------------------------

//********Измерение веса и запись в память************
void GetScale(){
  float _differenceWight;
  if (flgInitScale == true){
	  _differenceWight =  fabs(data.weight) - Scale.get_units(50);
    if (_differenceWight > 0.05 || _differenceWight < -0.05){
      data.weight = Scale.get_units(50);
      EEPROM.put(10, data.weight);
      //Serial.println (String (_differenceWight) + " кг. Разница!");
      //Serial.print (String (data.weight) + " кг. Записано в память!");
      //Serial.println ("");
    }
    else{
      //Serial.println (String (_differenceWight) + " кг. Разница!");
      //Serial.println (String (data.weight) + " кг. Вес!");
    }
  }
  else{
    _differenceWight = fabs(data.weight) - fabs(weightRAMInit + Scale.get_units(50));
    if (_differenceWight > 0.05 || _differenceWight < -0.05){
      data.weight = weightRAMInit + Scale.get_units(50);
      EEPROM.put(10, data.weight);
      //Serial.println (String (_differenceWight) + " кг. Разница!");
      //Serial.println (String (data.weight) + " кг. Сумма записана в память!");
      //Serial.println ("");
    }
    else{
      //Serial.println (String (_differenceWight) + " кг. Разница!");
      //Serial.println (String (data.weight) + " кг. Вес!");
    }
  } 
}
//-------------------------------------------------------------------------------------------

//*************Измерение напряжения на батареи***********************************
void GetCharge (){
  const float _K = 2*1.95;
  data.currentVoltage = _K*4.5f/1024*analogRead(PIN_BATTARY);
 }
//----------------------------------------------------------------------------------------------


//***************Отправка данных**************************************************
void SendData(){
  //-----Отправка веса-----
  structData _answer;
  int _counter = 0;
  NRF.powerUp();		//включение NRF
  delay(1000);
  while (_counter < 50){
    //Serial.println(counter);
    //Serial.println(data.weight);
    data.count = _counter;
    NRF.write(&data, sizeof(data));
    if(!NRF.available()){                     //если получаем пустой ответ
    }
	  else{  
      if(NRF.available()) {                      // если в ответе что-то есть
        NRF.read(&_answer, sizeof(_answer));                  // читаем
        Serial.print("Ответ "); Serial.println(_answer.count);
        if (data.weight == _answer.weight){
          _counter = 50;
        }
      }
    }  
	delay(1000);
	_counter++;
  }
NRF.powerDown(); 		//отключение NRF
}
//--------------------------------------------------------------------------------------------

//*************************Получение температуры и влажности с DHT11***************************
void GetTemp()
{
  //Serial.println("GetTempIN()>>>>>>>>>>>>>>");
  data.temp = SensorDHT.readTemperature();
  data.humidity = SensorDHT.readHumidity();
  //Serial.print("TempIN: ");Serial.println(sendData.temp);
  //Serial.print("HumidityIN: ");Serial.println(sendData.humidity);
 }
//-----------------------------------------------------------------------------------


void setup() {
  Serial.begin(9600);                                         // Скорость обмена данными с компьютером
  //Serial.println("Start!");
  
  power.autoCalibrate(); // автоматическая калибровка таймера сна (чтобы спал точное время)
  
  //---Вход для измерения напряжения---
  pinMode(PIN_BATTARY, INPUT);
  //--Инициализация датчика температуры
  SensorDHT.begin();
    
  //---Настройка весов---
  pinMode(3, INPUT);                   //PIN подключения кнопки
  EEPROM.get(10, weightRAMInit);               //Считывание данных с памяти  
  //Serial.print (String (weightRAMInit) + " кг. Прочитано из памяти!");
  //Serial.println ("");
  Scale.set_scale(CALIBRATION_FACTOR);              //Установка колибровочного коэффициента
  Scale.tare();                         //Сброс весов в 0
  
  //---Настройка модуля---
  NRF.begin(); //активировать модуль
  NRF.setAutoAck(1);         //режим подтверждения приёма, 1 вкл 0 выкл
  NRF.setRetries(0, 15);    //(время между попыткой достучаться, число попыток)
  NRF.enableAckPayload();    //разрешит отсылку данных в ответ на входящий сигнал
  NRF.setPayloadSize(32);     //размер пакета, в байтах
  NRF.openWritingPipe(address[1]);   //мы - труба 0, открываем канал для передачи данных
  NRF.setChannel(0x65);  //выбираем канал (в котором нет шумов!)
  NRF.setPALevel (RF24_PA_MAX); //уровень мощности передатчика. На выбор RF24_PA_MIN, RF24_PA_LOW, RF24_PA_HIGH, RF24_PA_MAX
  NRF.setDataRate (RF24_1MBPS); //скорость обмена. На выбор RF24_2MBPS, RF24_1MBPS, RF24_250KBPS
  //должна быть одинакова на приёмнике и передатчике!
  //при самой низкой скорости имеем самую высокую чувствительность и дальность!!
  NRF.powerUp(); //начать работу
  NRF.stopListening();  //не слушаем радиоэфир, мы передатчик
}

void loop() {
	//Serial.println("Loop");
	if (flgSetup == false){
	  delay(1000);
	}
	if (digitalRead(3) == HIGH){           //Если кнопка сброса нажата
		//Serial.println("Нажата кнопка сброса !");
		ROMinit();
		flgInitScale = true;         //Устанавливаем Флаг выбора программы измерений
	}
	//Измерение веса
	GetScale();         //Измерение веса
  GetCharge();
  GetTemp();
	if (millis() - myTimer1 >= 600000 || millis() < 600000) {   // таймер на 10 мин
		myTimer1 = millis();              // сброс таймера
		//Serial.println("10m");
		SendData();
	}
	if (millis() > 600000){
		//Serial.println("Sleep!");
		flgSetup = false;
		power.sleepDelay(60000);  // спим 60 секунд 
	}
}