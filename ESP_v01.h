/*
Прошивка для модуля ESP-07
Принимает данные от контролера arduino И отправляет на сервер BLYNK
*/
#define BLYNK_PRINT Serial


#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).
char auth[] = "1Heghni_p0iNSvOVq50JsdffALa1D4pVQ";

// Your WiFi credentials.
// Set password to "" for open networks.
char ssid[] = "SSID";
char pass[] = "QWERTY";

float Ves = 0;
float uBatt = 0;
int onLine = 99;

const byte numChars = 32;
char receivedChars[numChars];
char tempChars[numChars];    // временный массив используется во время парсинга

boolean newData = false;


//*******Получение данных по Serial**********


void GetSerialData() {
    static boolean recvInProgress = false;
    static byte ndx = 0;
    char startMarker = '<';
    char endMarker = '>';
    char rc;

    while (Serial.available() > 0 && newData == false) {
        rc = Serial.read();

        if (recvInProgress == true) {
            if (rc != endMarker) {
                receivedChars[ndx] = rc;
                ndx++;
                if (ndx >= numChars) {
                    ndx = numChars - 1;
                }
            }
            else {
                receivedChars[ndx] = '\0'; // завершаем строку
                recvInProgress = false;
                ndx = 0;
                newData = true;
            }
        }

        else if (rc == startMarker) {
            recvInProgress = true;
        }
    }
}

//============

void parseData() {      // разделение данных на составляющие части

    char * strtokIndx; // это используется функцией strtok() как индекс

    strtokIndx = strtok(tempChars, ";"); // получаем значение первой переменной
    onLine = atoi(strtokIndx);     // конвертируем эту составляющую в integer

    strtokIndx = strtok(NULL, ";");
    Ves = atof(strtokIndx);     // преобразовываем этот кусок текста в float
  
    strtokIndx = strtok(NULL, ";");
    uBatt = atof(strtokIndx);     // преобразовываем этот кусок текста в float
  

}

//============

void SendSerialData()
{
  Blynk.virtualWrite(V0, Ves);
  Blynk.virtualWrite(V1, uBatt);
  Blynk.virtualWrite(V2, onLine);
  //Serial.print("Ubat: ");//Serial.println(uBatt); 
      
}

void showParsedData() {
    Serial.print("onLine ");
    Serial.println(onLine);
    Serial.print("Ves ");
    Serial.println(Ves);
    Serial.print("Ubat ");
    Serial.println(uBatt);
}

void setup()
{
  // Debug console
  Serial.begin(9600);

  Blynk.begin(auth, ssid, pass);
}

void loop()
{
  Blynk.run();
  GetSerialData();
  if (newData == true) {
        strcpy(tempChars, receivedChars);
        parseData();
        //showParsedData();
        SendSerialData();
        newData = false;
    }

}
