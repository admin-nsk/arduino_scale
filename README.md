# arduino_scale
Project arduino scale
Это проект весов на arduino nano
Весы сосотоят из 2х контроллеров. 
Первый контроллер:
 - производит измерения с помощью HX711
 - передает данные через nRF24 на второй контроллер.
 
В задачи второго контроллера входит:
 - принимать сигналы от весов
 - обрабатывать входящие СМС запросы
 - отправка СМС с данными и уведомлениями о состоянии
 - пердача данных по UART модулю ESP-07
 
 В задачи ESP-07 входит:
  - получение данных по UART от контроллера
  - передача данных по Wi-Fi на сервер Blynk
  
This is a scale project on arduino nano
The scales consist of 2 controllers.
First controller:
  - makes measurements with HX711
  - transfers data via nRF24 to the second controller.
 
The tasks of the second controller include:
  - receive signals from scales
  - process incoming SMS requests
  - sending SMS with data and status notifications
  - data transmission via UART module ESP-07
 
  The tasks of the ESP-07 include:
   - receiving data via UART from the controller
   - data transfer via Wi-Fi to the Blynk server
