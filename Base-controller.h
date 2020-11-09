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
#include "nRF24L01.h"
#include "RF24.h"

SoftwareSerial SIM800(3, 2);                                  // RX, TX GSM
RF24 radio(9,10); // "создать" модуль на пинах 9 и 10

byte address[][6] = {"1Node","2Node","3Node","4Node","5Node","6Node"};  //возможные номера труб

float rdScale = 0;

String _response = "";                                     // Переменная для хранения ответа модуля

uint32_t RFTimer, RFTimer2;                      //таймер получения данных
bool flRX = false, flUp = true, flV = false;                     //флаги
float uBatt = 0;                          //Напряжение на батарейке
uint32_t myTimer1, myTimer2;   //Таймеры для loop
int crV = 10;              //Критически низкое напряжение на батарее

String phones = "+791345555654, +79615555656, +79135555599";   // Белый список телефонов
String msgphone;                        // Переменная для хранения номера отправителя
String msgbody;                         // Переменная для хранения текста СМС
String outSMS;

void(* resetFunc) (void) = 0;      //Функция перезагрузки

//***************************Отправка команды модему******************************
String SendATCommand(String cmd, bool waiting) {
  String _resp = "";                                              // Переменная для хранения результата
  //Serial.println(cmd);                                            // Дублируем команду в монитор порта
  SIM800.println(cmd);                                            // Отправляем команду модулю SIM
   
  if (waiting) {                                                  // Если необходимо дождаться ответа...
    _resp = WaitResponse();                                       // ... ждем, когда будет передан ответ
    // Если Echo Mode выключен (ATE0), то эти 3 строки можно закомментировать
    if (_resp.startsWith(cmd)) {                                  // Убираем из ответа дублирующуюся команду
      _resp = _resp.substring(_resp.indexOf("\r", cmd.length()) + 2);
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

bool hasmsg = false;                                              // Флаг наличия сообщений к удалению

//**********************Составление сообщения о весе и времени***********************
String ScaleSMS(){
  outSMS = "";
  int timeHours = (RFTimer2 / 3600ul);
  int timeMins = (RFTimer2 % 3600ul) / 60ul;
  int timeSecs = (RFTimer2 % 3600ul) % 60ul;
  outSMS = ("Ves: " + String (rdScale, 2) + " kg. - " + timeHours + "h.  " + timeMins + "m. " + timeSecs + "s.");
  //Serial.print ("msgphone:" + msgphone);
  //Serial.print (outSMS);
  return outSMS;
}
//-----------------------------------------------------------------------------------


//*************************Функция обаработки запроса в СМС***************************
void SMSSelect(String sms){
  //Serial.println("SMSSelect!"); 
  //int inSms = sms.toInt();
 Serial.println(sms);
   //switch (inSms) {
//+++++++++Запрос веса+++++++++++++
  
  if (sms == "1" || sms == "v" || sms == "V")
  {
   outSMS = ScaleSMS();
    SendSMS(msgphone, outSMS);
    //Serial.println("Запрос веса!");
  }
//+++++++++Перезагрузка+++++++++++++
   if (sms == "0" || sms == "r" || sms == "R")
  {
    //Serial.println("Reboot");
    String sSMS5 = ("Reboot!");
    SendSMS(msgphone, sSMS5);
    delay(15000);
    resetFunc();
  }
//+++++++++Качество сигнала+++++++++++++  
  if (sms == "3" || sms == "s" || sms == "S")
  {
    _response = SendATCommand("AT+CSQ", true);
    SendSMS(msgphone, _response);
  }
//+++++++++Напряжение на весах+++++++++++++
  if (sms == "4" || sms == "u" || sms == "U")
  {
    //Serial.println("Запрос напряжения");
    String sSMS2 = ("Napryagenie na vesah: " + String(uBatt) + "V.");
    SendSMS(msgphone, sSMS2);
  }
//+++++++++Напряжение на SIM800+++++++++++++
  if (sms == "5" || sms == "U800")
  {
    //Serial.println("Запрос напряжения!");
    _response = SendATCommand("AT+CBC", true);
    SendSMS(msgphone, _response); 
  }
}
//-----------------------------------------------------------------------------------

//**************************Проверка новых СМС***************************************
void CheckSMS (){
   //Serial.println("CheckSMS!");
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
      if (!hasmsg) hasmsg = true;                           // Ставим флаг наличия сообщений для удаления
        SendATCommand("AT+CMGR=" + (String)msgIndex, true);   // Делаем сообщение прочитанным
        SendATCommand("\n", true);                            // Перестраховка - вывод новой строки
        msgphone = ParseSMS(_response);                       // Отправляем текст сообщения на обработку
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
        if (hasmsg) {
          SendATCommand("AT+CMGDA=\"DEL READ\"", true);           // Удаляем все прочитанные сообщения
          hasmsg = false;
        }
        break;
      }
      
    } while (1);
}
//-----------------------------------------------------------------------------------

// *****************************Парсим SMS*******************************
String ParseSMS(String msg) {                                   
  String msgheader  = "";
  String msgbody    = "";
  //String msgphone   = "";
  //Serial.println("Parse!");
  msg = msg.substring(msg.indexOf("+CMGR: "));
  msgheader = msg.substring(0, msg.indexOf("\r"));            // Выдергиваем телефон

  msgbody = msg.substring(msgheader.length() + 2);
  msgbody = msgbody.substring(0, msgbody.lastIndexOf("OK"));  // Выдергиваем текст SMS
  msgbody.trim();

  int firstIndex = msgheader.indexOf("\",\"") + 3;
  int secondIndex = msgheader.indexOf("\",\"", firstIndex);
  msgphone = msgheader.substring(firstIndex, secondIndex);

  //Serial.println("Phone: " + msgphone);                       // Выводим номер телефона
  //Serial.println("Message: " + msgbody);                      // Выводим текст SMS

  if (msgphone.length() > 6 && phones.indexOf(msgphone) > -1) { // Если телефон в белом списке, то...
    //Serial.println("SMSSelectIN!");
    SMSSelect(msgbody);                           // ...выполняем команду
    
  }
  else {
    //Serial.println("Unknown phonenumber");
    }
  return msgphone;
}
//-----------------------------------------------------------------------------------

//****************************отправка SMS**************************************
void SendSMS(String phone, String message)
{
  //Serial.println("SendSMS!   " + phone+"    " + message);
  SendATCommand("AT+CMGS=\"" + phone + "\"", true);             // Переходим в режим ввода текстового сообщения
  SendATCommand(message + "\r\n" + (String)((char)26), true);   // После текста отправляем перенос строки и Ctrl+Z
}
//-----------------------------------------------------------------------------------
  
//***************************Получение данных от весов**************************
float ReadDataScl (){
  ////Serial.println("ReadDataScl!");
  float dataScl=0;
   int gotByte;  
    while(radio.available()){    // слушаем эфир со всех труб
    radio.read( &gotByte, 2 );         // чиатем входящий сигнал
    radio.writeAckPayload(1,&gotByte, 2 );  // отправляем обратно то что приняли
    //Serial.print("Recieved: "); //Serial.println(gotByte);
    if (gotByte < 20000){ 
      dataScl = float(gotByte) / 100;
      rdScale = dataScl;
      ////Serial.print("rdscale: ");//Serial.println(rdScale);
      // получаем из миллиса часы, минуты и секунды работы программы 
      // часы не ограничены, т.е. аптайм
      RFTimer = millis(); 
      flRX = false;
      //delay (1000);
    }
    if(gotByte >= 20000)
    {
      uBatt = (float(gotByte)-20000)/100;
      //Serial.print("Ubat: ");//Serial.println(uBatt); 
    }
    return dataScl;
  }
}
//-----------------------------------------------------------------------------------


//******************Отправка сообщения при долгом отсутствии сигнала от весов************
void ALARM (){ 
  //Serial.println("ALARM"); 
  RFTimer2 = (millis() - RFTimer) / 1000ul;  
  int timeHours2 = (RFTimer2 / 3600ul);
  if (timeHours2 >= 2 && flRX == false){  // Если входящего сигнала нет больше 2х часов
    flRX = true;                         //Сброс флага
    SendSMS ("+79137857684", "Net signala bolshe 2x chasov");  //Отправка СМС
  delay(5000);
  resetFunc();          //Перезагрузка
  } 
  if (flUp == true){          //Отправляем сообщение, что питание включено
    flUp = false;
    SendSMS ("+79137857684", "Pitanie vklucheno");
  
  }
  if (flV == false && uBatt > 0 && uBatt < crV){                //Если напряжение ниже критического и сообщение не отправлялось, то отправлем сообщение
    flV = true;
    SendSMS ("+79137857684", String("Batareya < ") + String(crV) + String ("V"));
  }
  if (uBatt > crV){     //Если напряжение стало больше критического, то сбрасываем флаг отправки сообщения
    flV = false;
  }
}
//-----------------------------------------------------------------------------------

void setup() {
  Serial.begin(9600);                                         // Скорость обмена данными с компьютером
  SIM800.begin(9600);                                         // Скорость обмена данными с модемом
    
  //Serial.println("Start!");
   
  //---Команды настройки GSM при каждом запуске---
  SendATCommand("AT", true);                                  // Отправили AT для настройки скорости обмена данными
  SendATCommand("AT+CMGDA=\"DEL ALL\"", true);               // Удаляем все SMS, чтобы не забивать память
  _response = SendATCommand("AT+CLIP=1", true);             // Включаем АОН
  _response = SendATCommand("AT+DDET=1", true);             // Включаем DTMF
  SendATCommand("AT+CMGF=1;&W", true);                        // Включаем текстовый режима SMS (Text mode) и сразу сохраняем значение (AT&W)!
  
  //---Настройка ESP---
  /*SendATCommand("AT+CWMODE=1", true, false);                                  //  Режим работы ESP station  mode
  SendATCommand("AT+CWJAP=" + wifiSSID + "," + wifipassword + "", true, false);           // Данные WiFi
  */
 
  
  //---Настройка nRF24L01----
  Serial.begin(9600); //открываем порт для связи с ПК
  radio.begin(); //активировать модуль
  radio.setAutoAck(1);         //режим подтверждения приёма, 1 вкл 0 выкл
  radio.setRetries(0,15);     //(время между попыткой достучаться, число попыток)
  radio.enableAckPayload();    //разрешить отсылку данных в ответ на входящий сигнал
  radio.setPayloadSize(32);     //размер пакета, в байтах

  radio.openReadingPipe(1,address[1]);      //хотим слушать трубу 0
  radio.setChannel(0x65);  //выбираем канал (в котором нет шумов!)

  radio.setPALevel (RF24_PA_MAX); //уровень мощности передатчика. На выбор RF24_PA_MIN, RF24_PA_LOW, RF24_PA_HIGH, RF24_PA_MAX
  radio.setDataRate (RF24_1MBPS); //скорость обмена. На выбор RF24_2MBPS, RF24_1MBPS, RF24_250KBPS
  //должна быть одинакова на приёмнике и передатчике!
  //при самой низкой скорости имеем самую высокую чувствительность и дальность!!
  // ВНИМАНИЕ!!! enableAckPayload НЕ РАБОТАЕТ НА СКОРОСТИ 250 kbps!
  
  radio.powerUp(); //начать работу
  radio.startListening();  //начинаем слушать эфир, мы приёмный модуль
  
}
//-----------------------------------------------------------------------------------

void loop() {
  if (millis() - myTimer1 >= 1000) {   // таймер на 1 сек 
    myTimer1 = millis();              // сброс таймера
    ReadDataScl();
    ALARM ();
    ////Serial.println("1s!");
  }
  if (millis() - myTimer2 >= 60000) {   // таймер на 1 мин
    myTimer2 = millis();              // сброс таймера
    CheckSMS ();
    //Serial.println("1m");
  }
}


