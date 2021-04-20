/* Version 02
 Это прошивка для принимающего модуля
 Его задачи:
 - Получения данных от весов
 - Обработка принятых СМС
 - Ответ в СМС
 - Передача данных модулю ESP для отправки на сервер Blynk

Модули:
	- nRF24L01
	- SIM800
  - ESP-07
	
Изменение функционала:
	
	- Исправлены ошибки отправки и подсчета таймера
*/

#include <SoftwareSerial.h>                                   // Библиотека програмной реализации обмена по UART-протоколу
#include <Arduino.h>
#include <SPI.h>
#include "RF24.h"

#define GSM_RX_PIN 2
#define GSM_TX_PIN 3
#define ESP_RX_PIN 5
#define ESP_TX_PIN 4
#define NRF24_CE_PIN 9
#define NRF24_CSN_PIN 10

SoftwareSerial SIM800(GSM_TX_PIN, GSM_RX_PIN);                                  // RX, TX GSM
RF24 NRF(NRF24_CE_PIN, NRF24_CSN_PIN); // "создать" модуль на пинах 9 и 10
SoftwareSerial ESP(ESP_TX_PIN, ESP_RX_PIN); 

byte address[][6] = {"1Node","2Node","3Node","4Node","5Node","6Node"};  //возможные номера труб

String _response = "";                                     // Переменная для хранения ответа модуля

struct  structData {
float weight;
float currentVoltage;
float temp;
float humidity;
int count;
} data;

uint32_t RFTimer, RFTimer2;                      //таймер получения данных
bool flagReciveData = false, flagPowerUp = true, flagAlarmVoltage = false;                     //флаги
uint32_t myTimer1, myTimer2, myTimer3;   //Таймеры для loop

const int CRITICAL_VOLTAGE = 10;              //Критически низкое напряжение на батарее
const String ALLOW_PHONE_NUMBERS = "+79137777777";   // Белый список телефонов
const String ALARM_PHONE = "+79137777777"; 

String senderPhone;                        // Переменная для хранения номера отправителя
String textSMS;

String serialStr;      //Строка для отправки ESP

void(* resetFunc) (void) = 0;      //Функция перезагрузки

//***************************Отправка команды модему******************************
String SendATCommand(String ATcommand, bool waiting) {
  SIM800.begin(9600);                                         // Скорость обмена данными с модемом
  String _resp = "";                                              // Переменная для хранения результата
  //Serial.println(ATcommand);                                            // Дублируем команду в монитор порта
  SIM800.println(ATcommand);                                            // Отправляем команду модулю SIM
   
  if (waiting) {                                                  // Если необходимо дождаться ответа...
    _resp = WaitResponse();                                       // ... ждем, когда будет передан ответ
    // Если Echo Mode выключен (ATE0), то эти 3 строки можно закомментировать
    if (_resp.startsWith(ATcommand)) {                                  // Убираем из ответа дублирующуюся команду
      _resp = _resp.substring(_resp.indexOf("\r", ATcommand.length()) + 2);
    }
    //Serial.println(_resp);                                        // Дублируем ответ в монитор порта
  }
  return _resp;                                                   // Возвращаем результат. Пусто, если проблема
}
//-----------------------------------------------------------------------------------

//***************Функция ожидания ответа и возврата полученного результата***************
String WaitResponse() {                                           // 
  String _resp = "";                                              // Переменная для хранения результата
   //Serial.println("Wait...");
  long _timeout = millis() + 10000;                               // Переменная для отслеживания таймаута (10 секунд)
 
  while (!SIM800.available() && millis() < _timeout)  {};         // Ждем ответа 10 секунд, если пришел ответ или наступил таймаут, то...
  if (SIM800.available()) {                                       // Если есть, что считывать...
    _resp = SIM800.readString();                                  // ... считываем и запоминаем
     //Serial.println(_resp);
  }
  else {                                                          // Если пришел таймаут, то...
    //Serial.println("Timeout...");                                 // ... оповещаем об этом и...
  }
 return _resp;                                                   // ... возвращаем результат. Пусто, если проблема
}
//-----------------------------------------------------------------------------------

bool existenceMsg = false;                                              // Флаг наличия сообщений к удалению

//**********************Составление сообщения о весе и времени***********************
void ScaleDataToSMS(){
  int _timeHours = (RFTimer2 / 3600ul);
  int _timeMins = (RFTimer2 % 3600ul) / 60ul;
  int _timeSecs = (RFTimer2 % 3600ul) % 60ul;
  textSMS = ("Ves: " + String (data.weight, 2) + " kg.");
  textSMS += _timeHours;
  textSMS += "h.  ";
  textSMS += _timeMins;
  textSMS += "m. ";
  textSMS += _timeSecs;
  textSMS += "s.\n";
  textSMS +="Temp: ";
  textSMS +=data.temp;
  textSMS += "\nHumidity: ";
  textSMS += data.humidity;
  textSMS += "%";
  textSMS += "\nUbat: ";
  textSMS += data.currentVoltage;
  textSMS += "V.";
  // Serial.print ("msgphone:" + senderPhone);
  // Serial.print (textSMS);
}
//-----------------------------------------------------------------------------------


//*************************Функция обаработки запроса в СМС***************************
void SMSSelect(String _textReceivedSMS){
  Serial.println("SMSSelect!"); 
  //Serial.println(_textReceivedSMS);
//+++++++++Запрос веса+++++++++++++
  
  if (_textReceivedSMS == "1" || _textReceivedSMS == "v" || _textReceivedSMS == "V")
  {
    ScaleDataToSMS();
    SendSMS(senderPhone, textSMS);
    // Serial.println("Запрос веса!");
  }
//+++++++++Перезагрузка+++++++++++++
   if (_textReceivedSMS == "0" || _textReceivedSMS == "r" || _textReceivedSMS == "R")
  {
    //Serial.println("Reboot");
    textSMS = ("Reboot!");
    SendSMS(senderPhone, textSMS);
    delay(15000);
    resetFunc();
  }
//+++++++++Качество сигнала+++++++++++++  
  if (_textReceivedSMS == "3" || _textReceivedSMS == "s" || _textReceivedSMS == "S")
  {
    textSMS = SendATCommand("AT+CSQ", true);
    SendSMS(senderPhone, textSMS);
  }
//+++++++++Напряжение на SIM800+++++++++++++
  if (_textReceivedSMS == "5" || _textReceivedSMS == "U800")
  {
    ////Serial.println("Запрос напряжения!");
    textSMS = SendATCommand("AT+CBC", true);
    SendSMS(senderPhone, textSMS); 
  }
  //+++++++++Перезагрузка ESP+++++++++++++
   if (_textReceivedSMS == "E" || _textReceivedSMS == "e")
  {
    // Serial.println("Reboot ESP");
    digitalWrite(6, LOW);
    digitalWrite(6, HIGH);
  }
    //+++++++++Отправка данных на ESP+++++++++++++
  if (_textReceivedSMS == "se" || _textReceivedSMS == "SE")
  {
	  // Serial.println("Send ESP>>>>>");
	  SerialESP();
  }
}
//-----------------------------------------------------------------------------------

//**************************Проверка новых СМС***************************************
void CheckSMS (){
   Serial.println("CheckSMS!");
    do {
      _response = SendATCommand("AT+CMGL=\"REC UNREAD\",1", true);// Отправляем запрос чтения непрочитанных сообщений
      if (_response.indexOf("+CMGL: ") > -1) {                    // Если есть хоть одно, получаем его индекс
        int msgIndex = _response.substring(_response.indexOf("+CMGL: ") + 7, _response.indexOf("\"REC UNREAD\"", _response.indexOf("+CMGL: ")) - 1).toInt();
        char i = 0;                                               // Объявляем счетчик попыток
        do {
          i++;                                                    // Увеличиваем счетчик
          _response = SendATCommand("AT+CMGR=" + (String)msgIndex + ",1", true);  // Пробуем получить текст SMS по индексу
          _response.trim();                                       // Убираем пробелы в начале/конце
          if (_response.endsWith("OK")) {                         // Если ответ заканчивается на "ОК"
      if (!existenceMsg) existenceMsg = true;                           // Ставим флаг наличия сообщений для удаления
        SendATCommand("AT+CMGR=" + (String)msgIndex, true);   // Делаем сообщение прочитанным
        SendATCommand("\n", true);                            // Перестраховка - вывод новой строки
        ParseSMS(_response);                       // Отправляем текст сообщения на обработку
        break;                                                // Выход из do{}
      }
      else {                                                  // Если сообщение не заканчивается на OK
            //Serial.println ("Error answer");                      // Какая-то ошибка
        SendATCommand("\n", true);                            // Отправляем новую строку и повторяем попытку
      }
    } while (i < 10);
        break;
      }
      else {
        if (existenceMsg) {
          SendATCommand("AT+CMGDA=\"DEL READ\"", true);           // Удаляем все прочитанные сообщения
          existenceMsg = false;
        }
        break;
      }
      
    } while (1);
}
//-----------------------------------------------------------------------------------

// *****************************Парсим SMS*******************************
void ParseSMS(String receivedSMS) {                                   
  String _msgHeader  = "";
  String _textReceivedSMS    = "";
  // Serial.println("Parse!");
  receivedSMS = receivedSMS.substring(receivedSMS.indexOf("+CMGR: "));
  _msgHeader = receivedSMS.substring(0, receivedSMS.indexOf("\r"));            // Выдергиваем телефон

  _textReceivedSMS = receivedSMS.substring(_msgHeader.length() + 2);
  _textReceivedSMS = _textReceivedSMS.substring(0, _textReceivedSMS.lastIndexOf("OK"));  // Выдергиваем текст SMS
  _textReceivedSMS.trim();

  int _firstIndex = _msgHeader.indexOf("\",\"") + 3;
  int _secondIndex = _msgHeader.indexOf("\",\"", _firstIndex);
  senderPhone = _msgHeader.substring(_firstIndex, _secondIndex);

  Serial.println("Phone: " + senderPhone);                       // Выводим номер телефона
  Serial.println("Message: " + _textReceivedSMS);                      // Выводим текст SMS

  if (senderPhone.length() > 6 && ALLOW_PHONE_NUMBERS.indexOf(senderPhone) > -1) { // Если телефон в белом списке, то...
    //Serial.println("SMSSelectIN!");
    SMSSelect(_textReceivedSMS);                           // ...выполняем команду
    
  }
  else {
    //Serial.println("Unknown phonenumber");
    }
}
//-----------------------------------------------------------------------------------

//****************************отправка SMS**************************************
void SendSMS(String receiverPhone, String textSMS)
{
  Serial.println("SendSMS!   " + receiverPhone+"    " + textSMS);
  SendATCommand("AT+CMGS=\"" + receiverPhone + "\"", true);             // Переходим в режим ввода текстового сообщения
  SendATCommand(textSMS + "\r\n" + (String)((char)26), true);   // После текста отправляем перенос строки и Ctrl+Z
}
//-----------------------------------------------------------------------------------
  
//***************************Получение данных от весов**************************
void ReadDataScl (){
  NRF.startListening();
  // Serial.println("ReadData!");
  while(NRF.available()){    // слушаем эфир со всех труб
    NRF.read( &data, sizeof(data) );         // чиатем входящий сигнал
    Serial.print("Recieved: "); Serial.println(data.count);
    Serial.print("weight: ");Serial.println(data.weight);
    NRF.writeAckPayload(1,&data, sizeof(data) );  // отправляем обратно то что приняли
    // получаем из миллиса часы, минуты и секунды работы программы 
    // часы не ограничены, т.е. аптайм
    // NRF.stopListening();
    // NRF.write(&data, sizeof(data));
    RFTimer = millis(); 
    flagReciveData = false;
    // Serial.print("Ubat: ");Serial.println(data.currentVoltage); 
  }
}
//-----------------------------------------------------------------------------------


//******************Отправка сообщения при долгом отсутствии сигнала от весов************
void ALARM (){ 
  ////Serial.println("ALARM"); 
  RFTimer2 = (millis() - RFTimer) / 1000ul;  
  int _timerHours = (RFTimer2 / 3600ul);
  if (_timerHours >= 2 && flagReciveData == false){  // Если входящего сигнала нет больше 2х часов
    flagReciveData = true;                         //Сброс флага
    SendSMS (ALARM_PHONE, "Net signala bolshe 2x chasov");  //Отправка СМС
    delay(5000);
    resetFunc();          //Перезагрузка
  } 
  if (flagPowerUp == true){          //Отправляем сообщение, что питание включено
    flagPowerUp = false;
    SendSMS (ALARM_PHONE, "Pitanie vklucheno");
  
  }
  if (flagAlarmVoltage == false && data.currentVoltage > 0 && data.currentVoltage < CRITICAL_VOLTAGE){                //Если напряжение ниже критического и сообщение не отправлялось, то отправлем сообщение
    flagAlarmVoltage = true;
    SendSMS (ALARM_PHONE, String("Batareya < ") + String(CRITICAL_VOLTAGE) + String ("V"));
  }
  if (data.currentVoltage > CRITICAL_VOLTAGE){     //Если напряжение стало больше критического, то сбрасываем флаг отправки сообщения
    flagAlarmVoltage = false;
  }
}
//-----------------------------------------------------------------------------------

void SIMinit()
{
  Serial.println("SIMInit>>>>>");
  SIM800.begin(9600);                                         // Скорость обмена данными с модемом

  ////Serial.println("Start!");
   data.currentVoltage = 0;
  //---Команды настройки GSM при каждом запуске---
  SendATCommand("AT", true);                                  // Отправили AT для настройки скорости обмена данными
  SendATCommand("AT+CMGDA=\"DEL ALL\"", true);               // Удаляем все SMS, чтобы не забивать память
  SendATCommand("AT+CLIP=1", true);             // Включаем АОН
  SendATCommand("AT+DDET=1", true);             // Включаем DTMF
  SendATCommand("AT+CMGF=1;&W", true);                        // Включаем текстовый режима SMS (Text mode) и сразу сохраняем значение (AT&W)!
}

//*************************Инициализация NRF******************************************
void NRFinit()
{
  Serial.println("NRFInit>>>>>");
   //---Настройка nRF24L01----
  NRF.begin(); //активировать модуль
  NRF.setAutoAck(1);         //режим подтверждения приёма, 1 вкл 0 выкл
  NRF.setRetries(0,15);     //(время между попыткой достучаться, число попыток)
  NRF.enableAckPayload();    //разрешить отсылку данных в ответ на входящий сигнал
  NRF.setPayloadSize(32);     //размер пакета, в байтах

  NRF.openReadingPipe(1,address[5]);      //хотим слушать трубу 0
  NRF.setChannel(0x65);  //выбираем канал (в котором нет шумов!)

  NRF.setPALevel (RF24_PA_MAX); //уровень мощности передатчика. На выбор RF24_PA_MIN, RF24_PA_LOW, RF24_PA_HIGH, RF24_PA_MAX
  NRF.setDataRate (RF24_1MBPS); //скорость обмена. На выбор RF24_2MBPS, RF24_1MBPS, RF24_250KBPS
  //должна быть одинакова на приёмнике и передатчике!
  //при самой низкой скорости имеем самую высокую чувствительность и дальность!!
  // ВНИМАНИЕ!!! enableAckPayload НЕ РАБОТАЕТ НА СКОРОСТИ 250 kbps!
  
  NRF.powerUp(); //начать работу
  NRF.startListening();  //начинаем слушать эфир, мы приёмный модуль
}

//*************************Отправка данных в ESP*************************************
void SerialESP ()
{
  ESP.begin(9600);
  ESP.listen();
  Serial.println("SerialESP>>>>>");
  delay(1000);
  int _timeHours = (RFTimer2 / 3600ul);       //Вычисляем часы
  int _timeMins = (RFTimer2 % 3600ul) / 60ul;     //Вычисляем минуты
  _timeMins = _timeMins + (_timeHours * 60ul);
  serialStr = "<";                  //составляем строку для отправки
  serialStr += _timeMins;
  serialStr += ";";
  serialStr += data.weight;
  serialStr += ";";
  serialStr += data.currentVoltage;
  serialStr += ">";
  if (ESP.isListening())
  {
    Serial.print("serialStr:");Serial.println(serialStr);
    //Serial.print("buf:");Serial.println(serialStr.c_str());
    ESP.println(serialStr);
    ESP.write(serialStr.c_str());         //отправляем строку
    delay(50);
  }
  ESP.end();
}
//-------------------------------------------------------------------------------------------------


void setup() {
  Serial.begin(9600);                                         // Скорость обмена данными с компьютером
    
  //Serial.println("Start!");
  data.currentVoltage = 0;
  
  //-------------------Выходы сброса модулей---------------------
  pinMode(6, OUTPUT);                   //Сброс ESP
  pinMode(7, OUTPUT);                   //Сброс SIM800
  digitalWrite(6, HIGH);
  digitalWrite(7, HIGH);
  delay(50);
  //---Команды настройки GSM при каждом запуске---
  SIMinit();

  //---Настройка nRF24L01----
  NRFinit();
}
//-----------------------------------------------------------------------------------

void loop() {
  if (millis() - myTimer1 >= 1000) {   // таймер на 1 сек 
    myTimer1 = millis();              // сброс таймера
    ReadDataScl();
    ALARM ();
    //Serial.println("1s!");
  }
  if (millis() - myTimer2 >= 60000) {   // таймер на 1 мин
    myTimer2 = millis();              // сброс таймера
    CheckSMS();
    //Serial.println("1m");
  }
  if (millis() - myTimer3 >= 600000) {   // таймер на 10 мин
    myTimer3 = millis();              // сброс таймера
    SerialESP();
    //Serial.println("10m");
  }
}


