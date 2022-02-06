//
// Программа блока управления многофункциональным антенным коммутатором с коммутацией 3 антенных входов и 2 трансиверов
// с возможностью коммутации через усилитель мощности, с блокировками коммутации, запретом передачи на втором трансивере
// Управление контактными кнопками, индикация на дисплее LCD 1602 с подключением по I2C
// R3TJL (c) 2022 V1.0
//                                                                                                                         
//**************************************************************************************************************************
#include <Wire.h>                                                                 //подключаем библиотеку Wire для I2C
#include <LiquidCrystal_I2C.h>                                                    //подключаем библиотеку для работы с ЖКИ LCD1602 i2c
LiquidCrystal_I2C lcd(0x27,16,2);                                                    // Устанавливаем дисплей 16*2
//#include <TonePlayer.h>                                                           //подключаем библиотеку вывода тона
//#include <sav_button.h>                                                           //подключаем библиотеку кнопок
#include <EEPROM.h>                                                               //подключаем библиотеку для работы с EEPROM
#include <CyberLib.h>                                                             //очень интересная библиотека, заменяет многое работа с пинами, с кнопками, со звуком и прочее...

//***************************Определяем имена пинам управления релюшками******************************************************
#define rel_1 8
#define rel_2 10
#define rel_3 5
#define rel_4 6
#define rel_5 9
#define rel_6 7
#define rel_7 4
#define rel_8 12
//*************Определяем имена прочих пинов входов и выходов***********************
#define ptt1 2
#define ptt2 3
#define tx1 14
#define tx2 15
#define mode_key A2
#define ant_key A3

#define tone_beep 1000                                                              //тональность бипера (1000-1кгц)

//#define DS_UPDATE_TIME 500                                                        //время цикла опроса датчика температуры
//#define res 5                                                                     //порт D1, сброс (очистка) памяти 
//#define FanPin 4                                                                  //порт D4 для подключения вентилятора охлаждения
//#define TFanOn 40.0                                                               //температура включения вентилятора
//#define TFanOff 35.0                                                              //температура выключения вентилятора
//#define DivPin 12                                                                 //порт D12 для подключения делителя напряжения прямой и обратной шкалы
//#define TFT_RST 7                                                                 //7 вывод Nano на RST ЖКИ
//#define TFT_RS  8                                                                 //8 вывод Nano на RS ЖКИ
//#define TFT_CS  10                                                                //10 вывод Nano на CS ЖКИ
//#define TFT_SDI 11                                                                //11 вывод Nano на SDI ЖКИ
//#define TFT_CLK 13                                                                //13 вывод Nano на CLK ЖКИ
//#define TFT_LED 6                                                                 //6 вывод ШИМ для подсветки
//#define Bmin 30                                                                   //уровень яркости во время заставки
//#define Bmax 150                                                                  //уровень яркости во время измерения
//#define COLOR_SCALE COLOR_WHITE                                                   //цвет шкалы линий КСВ и измерителя мощности
//TFT_22_ILI9225 tft = TFT_22_ILI9225(TFT_RST, TFT_RS, TFT_CS, TFT_LED, Bmax);      //инициализация аппаратного SPI
//TonePlayer tone1 (TCCR1A,TCCR1B,OCR1AH,OCR1AL,TCNT1H,TCNT1L);                     //используем порт D9 (Таймер 1) для подключения бипера и звуковой сигнализации (Nano)
//SButton button1(2,20,500,1000,0);                                                 //назначаем кнопку 1 на порт D2
//SButton button2(3,20,500,1000,250);                                               //назначаем кнопку 2 на порт D3

//************************* Назначаем переменные ***********************************
  int Trc1, Trc2 = 0;                                                             //переменная состояния линий Трансивер1 и Трансивер2. 0 - не подкл. 1 - к А1. 2 - к А2. 3 - к А3
  int PAline = 0;                                                                   //переменная подключения усилителя 0 - не подкл. 1 - к Тр1. 2 - к Тр2
  volatile int trx_upr_mode;                            //переменная режима управления передачей трансиверов: 0-без упр., 1-только блок., 2-упр.по текущему выбору, 3-полное управление
                                                        //переменная используется в прерываниях

// переменные блока опроса кнопок управления
  boolean press_flag = false;        
  boolean long_press_flag = false;   
  unsigned long last_press = 0;
  int value;   
  int last_value = 1023;
  int last_a = 1023;
  int key_ant;                //это ЗНАЧЕНИЕ кнопки выбора антенны (1, 2, 3; 0-не нажата)
  int key_mode;               //это ЗНАЧЕНИЕ кнопки режима (короткие нажатия 1, 2, 3; долгие нажатия 4, 5, 6; 0-не нажата)
//***********************

//*********************************** Установки ***********************************
void setup() {
  analogReference(DEFAULT);                                                     //подключаем внутреннее опорное напряжение 5В
//    analogReference(INTERNAL);                                                    //подключаем внутреннее опорное напряжение 1.1В
//    button1.begin(); button2.begin();                                             //инициализация кнопки 1 и кнопки 2
  pinMode(rel_1,OUTPUT);                                                        //назначаем пины на входы и выходы
  pinMode(rel_2,OUTPUT);  
  pinMode(rel_3,OUTPUT);
  pinMode(rel_4,OUTPUT);
  pinMode(rel_5,OUTPUT);
  pinMode(rel_6,OUTPUT);
  pinMode(rel_7,OUTPUT);
  pinMode(rel_8,OUTPUT);

  pinMode(ptt1,INPUT);
  pinMode(ptt2,INPUT);

  pinMode(tx1,OUTPUT);
  pinMode(tx2,OUTPUT);

  pinMode(mode_key,INPUT);     //надо ли так конфигурировать вход для аналогового?
  pinMode(ant_key,INPUT);

//**************прерывания на входах 2 и 3
attachInterrupt(0, tx_on_block, LOW);
attachInterrupt(1, tx_on_block, LOW);
  

//  if (digitalRead(res)==LOW){                                                     //определяем состояние кнопки сброса res
//   while (digitalRead(res)==LOW){}                                                 //зацикливаемся, если кнопка еще не отпущена
//    EEPROM.update(1,0);                                                           //записываем в память значения по умолчанию
//    EEPROM.update(2,0);                                                           //----------
//    EEPROM.update(3,0);                                                           //----------
//    EEPROM.put(4,2.5);                                                            //----------
//    EEPROM.put(8,3.0);                                                            //----------
//    EEPROM.update(12,50);                                                         //----------
//    EEPROM.update(13,10);                                                         //----------
//    EEPROM.update(14,20);}                                                        //----------
//*****************************Заставка приветствия*********************************************
lcd.init();                     
  lcd.backlight();                                                            // Включаем подсветку дисплея
  lcd.print("Ant.commutator");
  lcd.setCursor(8, 1);
  lcd.print("LCD 1602");

trx_upr_mode=EEPROM.read(1);
if (trx_upr_mode < 0 || trx_upr_mode > 3) {
  lcd.clear();
  lcd.print("Please set mode");
  lcd.setCursor(0,1); 
  lcd.print("TX Transceiver !");
  beeper_err();
  delay_ms(2000);
  trx_upr_mode = 1; 
  //EEPROM.write(1,1);
}

}
//=============================== Основной цикл программы =====================================
void loop() {
  Start
    digitalWrite(tx1,HIGH);
    //analogRead(ant_key);
    beeper();  
    beeper_err();
    Trc1 = find_similar(analogRead(ant_key), 10, 100);


//****************************функция опроса кнопок***********************************
// НЕЛЬЗЯ ВЫНОСИТЬ В ФУНКЦИЮ !!!! Переменные определяем как глобальные? 

//считываем значение от кнопок выбора антенного входа (только короткие)
  value = find_similar(analogRead(ant_key), 10, 100);   //вопрос - работает ли эта функция от дребезга библиотеки CyberLib.h ???
                                                        //иначе - простое считывание analogRead(ant_key)
    if (value > 900) {
      key_ant = 0;}
    if (value < 100 && last_a > 900) { 
      key_ant = 3;
      beeper();}
    if (value > 100 && value < 400 && last_a > 900) { 
      key_ant = 2;
      beeper();}
    if (value >400 && value < 900 && last_a > 900) { 
      key_ant = 1;
      beeper();}
    last_a = value;

//считываем значения от кнопок выбора режима (короткие и длинные)
  value = find_similar(analogRead(mode_key), 10, 100);
    if (value > 900 && press_flag == false && long_press_flag == false) {
      key_mode = 0;    }
    //если нажата кнопка 3
    if (value < 100 && press_flag == false && millis() - last_press > 100) {
      // если кнопка была нажата и не была нажата до этого (флажок короткого нажатия = false)
      // и с последненго нажатия прошло более 100 миллисекунд (защита от дребезга контактов) то...
        press_flag = !press_flag;       //...поднять флажок короткого нажатия на кнопку и
        last_press = millis();          // присвоить текущее время переменной last_press
        last_value = value;
    }
    else if (value < 100 && press_flag == true && millis() - last_press > 1000) {
      // если кнопку продолжают нажимать более 1 секунды, то...
      long_press_flag = !long_press_flag;  // ...поднять флажок долгого нажатия и
      last_press = millis();          // присвоить текущее время переменной last_press
      // Сюда вписываем события неоходимые при длительном нажатии на кнопку
      key_mode = 6;
      beeper_long();
    }
    else if (value > 900 && last_value < 100 && press_flag == true && long_press_flag == false) {
      // если кнопка отпущена и было только короткое нажатие, то...
      press_flag = !press_flag;  // опустить флажок короткого нажатия
      last_press = 0;
      last_value = 1023;
      // сюда вписываем события неоходимые при коротком нажатии на кнопку
      key_mode = 3;
      beeper();
    }    
    else if (value > 900 && press_flag == true && long_press_flag == true) {
      // если кнопка отпушена и была нажата длительное время, то...
      press_flag = !press_flag;            // опустить флажок короткого нажатия
      long_press_flag = !long_press_flag;  // и длинного нажатия
      last_press = 0;
      last_value = 1023;
    }

    //если нажата кнопка 2
     if (value > 100 && value < 400 && press_flag == false && millis() - last_press > 100) {
      // если кнопка была нажата и не была нажата до этого (флажок короткого нажатия = false)
      // и с последненго нажатия прошло более 100 миллисекунд (защита от дребезга контактов) то...
        press_flag = !press_flag;       //...поднять флажок короткого нажатия на кнопку и
        last_press = millis();          // присвоить текущее время переменной last_press
        last_value = value;
    }
    else if (value > 100 && value < 400 && press_flag == true && millis() - last_press > 1000) {
      // если кнопку продолжают нажимать более 1 секунды, то...
      long_press_flag = !long_press_flag;  // ...поднять флажок долгого нажатия и
      last_press = millis();          // присвоить текущее время переменной last_press
      // Сюда вписываем события неоходимые при длительном нажатии на кнопку
      key_mode = 5;
      beeper_long();
    }
    else if (value > 900 && last_value > 100 && last_value < 400 && press_flag == true && long_press_flag == false) {
      // если кнопка отпущена и было только короткое нажатие, то...
      press_flag = !press_flag;  // опустить флажок короткого нажатия
      last_press = 0;
      last_value = 1023;
      // сюда вписываем события неоходимые при коротком нажатии на кнопку
      key_mode = 2;
      beeper();
    }
      //если нажата кнопка 1
     if (value >400 && value < 900 && press_flag == false && millis() - last_press > 100) {
      // если кнопка была нажата и не была нажата до этого (флажок короткого нажатия = false)
      // и с последненго нажатия прошло более 100 миллисекунд (защита от дребезга контактов) то...
        press_flag = !press_flag;       //...поднять флажок короткого нажатия на кнопку и
        last_press = millis();          // присвоить текущее время переменной last_press
        last_value = value;
    }
    else if (value >400 && value < 900 && press_flag == true && millis() - last_press > 1000) {
      // если кнопку продолжают нажимать более 1 секунды, то...
      long_press_flag = !long_press_flag;  // ...поднять флажок долгого нажатия и
      last_press = millis();          // присвоить текущее время переменной last_press
      // Сюда вписываем события неоходимые при длительном нажатии на кнопку
      key_mode = 4;
      beeper_long();
    }
    else if (value > 900 && last_value > 400 && last_value < 900 && press_flag == true && long_press_flag == false) {
      // если кнопка отпущена и было только короткое нажатие, то...
      press_flag = !press_flag;  // опустить флажок короткого нажатия
      last_press = 0;
      last_value = 1023;
      // сюда вписываем события неоходимые при коротком нажатии на кнопку
      key_mode = 1;
      beeper();
    }
//************************конец блока опроса кнопок управления 




//    int U1=0,U2=0;                                                                //назначаем переменные прямой и обратной волны
//    const int COUNT = 10;                                                         //назначаем константу кол-во измерений
//  for (byte i=0; i<COUNT; i++){                                                   //включаем цикл измерения
//    U1 +=  A0_Read;                                                               //суммируем значения U1 с порта А0
//    U2 +=  A1_Read;}                                                              //суммируем значения U2 с порта А1
//    U1 = U1 / COUNT;                                                              //усредняем значение U1
//    U2 = U2 / COUNT;                                                              //усредняем значение U2
//  if (U1<10) U1=0;                                                                //ограничение малых значений U1
//  if (U2<10) U2=0;                                                                //ограничение малых значений U2
//
//switch(button1.Loop()){                                                           //определяем состояние кнопки 1
//  case SB_CLICK: beeper(); U1=10; break;                                          //по короткому нажатию запускаем бипер, уст. U1=10
//  case SB_LONG_CLICK: beeper(); menu(); break;}                                   //по длинному нажатию запускаем бипер, вход в меню установок
//
//switch(button2.Loop()){                                                           //определяем состояние кнопки 2
//  case SB_CLICK: beeper(); U1=10; break;                                          //по короткому нажатию запускаем бипер, уст. U1=10
//  case SB_LONG_CLICK: beeper(); menu(); break;}                                   //по длинному нажатию запускаем бипер, вход в меню установок
//  
//  if (flag_clock==0){                                                             //режим заставки
//    int cx=5;                                                                     //начальная координата по х для вывода текста
//    int cy=60;                                                                    //начальная координата по y для вывода текста
//    tft.drawGFXText(cx,cy,"SWR & PWR-Meter v.2",COLOR_RED);                       //выводим текст "SWR & PWR-Meter v.2" красным цветом
//    tft.drawGFXText(cx+25,cy+30,"Snezhnoe, 2019",COLOR_RED);                      //выводим текст "Snezhnoe, 2018" красным цветом
//    tft.drawGFXText(cx+75,cy+60,"D0ISM",COLOR_RED);}                               //выводим текст "D0ISM" красным цветом
//  
//  if (flag_clock==1){                                                             //в режиме измерения
//  if (LCD==1){LCD++; start_LCD();}                                                //однократное выполнение очистки ЖКИ и запускаем экран
//    swr(U1,U2);                                                                   //вызываем подпрограмму расчета и вывода КСВ
//    pwr (U1);}                                                                     //вызываем подпрограмму расчета и вывода мощности прямой волны
//    temp();
////************************* Управление индикацией ЖКИ **********************************
//  if ((U1<10)&&(flag_clock==1)){                                                  //если напряжение прямой волны U1 меньше 10 и в режиме измерения
//  if (millis()-clockMillis>=1){clockMillis=millis(); val_clock++;}                //начинаем считать время, увеличиваем val_clock каждую миллисекунду
//  if ((clockTime-val_clock<=Bmax)&&(clockTime-val_clock>=Bmin)){                  //при равенстве счетчика val_clock до зн-я Bmax и до Bmin
//    tft.setBacklightBrightness(clockTime-val_clock);}}                            //уменьшаем яркость подсветки с каждым значением val_clock
//  else {val_clock=0;}                                                             //иначе обнуляем счетчик val_clock
//  if (val_clock>=clockTime){                                                      //если счетчик val_clock сравнялся со значением clockTime
//    val_clock=0; flag_clock=0; LCD=1; tft.clear();}                               //обнуляем счетчик val_clock, режим заставки, взводим счетчик LCD, очистка ЖКИ
//  if (U1>=10){flag_clock=1; tft.setBacklightBrightness(Bmax);}                    //если напряжение прямой волны U1>=10, переходим в режим измерения, яркость подсветки Bmax
End
}
//===============================================================================================


//******************************* Подпрограмма расчета и вывода КСВ ***************************************************************  

////************************** Подпрограмма расчета и вывода мощности прямой волны ***************************************************
//void pwr (float U1){                                                              //подпрограмма мощности прямой волны с привязкой по U1
//    char buf[4]; byte len = 4;
//    int R,G,B;                                                                    //переменные цветов
//  if (flag_power==0){                                                             //проверяем флаг состояния мощности
//  if (flag_scale==1){flag_scale++;                                                //проверяем флаг состояния шкалы, увеличиваем его на 1
//    tft.fillRectangle(5,122,215,136,COLOR_BLACK);                                 //стираем старую шкалу черным цветом
//
//  if (Scale_low==0){                                                              //если выбрана входная шкала 0
//    tft.drawGFXText(6,  135, "0", COLOR_SCALE);                                   //выводим текст "0"
//    tft.drawGFXText(48, 135, "5", COLOR_SCALE);                                   //выводим текст "10"
//    tft.drawGFXText(94, 135, "10", COLOR_YELLOW);                                 //выводим текст "20"
//    tft.drawGFXText(144,135, "15", COLOR_ORANGE);                                 //выводим текст "30"
//    tft.drawGFXText(190,135, "20", COLOR_RED);}                                   //выводим текст "40"
//
//  if (Scale_low==1){                                                              //если выбрана входная шкала 1
//    tft.drawGFXText(6,  135, "0", COLOR_SCALE);                                   //выводим текст "0"
//    tft.drawGFXText(44, 135, "10", COLOR_SCALE);                                  //выводим текст "10"
//    tft.drawGFXText(94, 135, "20", COLOR_YELLOW);                                 //выводим текст "20"
//    tft.drawGFXText(144,135, "30", COLOR_ORANGE);                                 //выводим текст "30"
//    tft.drawGFXText(190,135, "40", COLOR_RED);}                                   //выводим текст "40"
//
//  if (Scale_low==2){                                                              //если выбрана входная шкала 2
//    tft.drawGFXText(6,  135, "0", COLOR_SCALE);                                   //выводим текст "0"
//    tft.drawGFXText(44, 135, "20", COLOR_SCALE);                                  //выводим текст "20"
//    tft.drawGFXText(94, 135, "40", COLOR_YELLOW);                                 //выводим текст "40"
//    tft.drawGFXText(144,135, "60", COLOR_ORANGE);                                 //выводим текст "60"
//    tft.drawGFXText(190,135, "80", COLOR_RED);}                                   //выводим текст "80"
//
//  if (Scale_low==3){                                                              //если выбрана входная шкала 3
//    tft.drawGFXText(6,  135, "0", COLOR_SCALE);                                   //выводим текст "0"
//    tft.drawGFXText(44, 135, "30", COLOR_SCALE);                                  //выводим текст "40"
//    tft.drawGFXText(94, 135, "60", COLOR_YELLOW);                                 //выводим текст "60"
//    tft.drawGFXText(144,135, "90", COLOR_ORANGE);                                 //выводим текст "80"
//    tft.drawGFXText(178,135, "120", COLOR_RED);}}                                 //выводим текст "120"
//   
//    V=((U1*5)/1022)*l;                                                            //приводим напряжение к мощности
//    PWR=(V*V)/50;                                                                 //вычисляем мощность
//    digitalWrite(DivPin,LOW);                                                     //устанавливаем низкий уровень делителя DivPin
//  
//  if (PWR<9.9){                                                                   //если мощность меньше 10Вт
//    dtostrf(PWR, len, 1, buf);                                                    //преобразуем значение PWR в массив buf
//    buf[len] = 'W'; buf[len+1] = 0;                                               //добавляем к массиву знак W
//    tft.drawText(158,8,buf+String (' '),COLOR_GREEN);}                            //выводим значение мощности с пробелом в конце
//
//  if (PWR>=10){                                                                   //если мощность больше или равна 10Вт
//    dtostrf(PWR, len, 0, buf);                                                    //преобразуем значение PWR в массив buf
//    buf[len] = 'W'; buf[len+1] = 0;                                               //добавляем к массиву знак W
//    tft.drawText(158,8,buf+String (' '),COLOR_GREEN);}                            //выводим значение мощности с пробелом в конце
//  
//  if (PWR>=PWRthreshold){flag_power=1; flag_scale=1; digitalWrite(DivPin,HIGH);}} //если мощность больше PWRthreshold, флаг шкалы 1, уст. 1 на внешнем делителе напряжения
//
//  if (flag_power==1){                                                             //проверяем флаг состояния мощности
//  if (flag_scale==1){flag_scale++;                                                //проверяем флаг состояния шкалы, увеличиваем его на 1
//    tft.fillRectangle(5,122,215,136,COLOR_BLACK);                                 //стираем старую шкалу черным цветом
//
//  if (Scale_hi==0){                                                               //если выбрана выходная шкала 0
//    tft.drawGFXText(6,  135, "0", COLOR_SCALE);                                   //выводим текст "0"
//    tft.drawGFXText(44, 135, "50", COLOR_SCALE);                                  //выводим текст "50"
//    tft.drawGFXText(87, 135, "100", COLOR_YELLOW);                                //выводим текст "100"
//    tft.drawGFXText(137,135, "150", COLOR_ORANGE);                                //выводим текст "150"
//    tft.drawGFXText(178,135, "200", COLOR_RED);}                                  //выводим текст "200"
//
//  if (Scale_hi==1){                                                               //если выбрана выходная шкала 1
//    tft.drawGFXText(6,  135, "0", COLOR_SCALE);                                   //выводим текст "0"
//    tft.drawGFXText(39, 135, "100", COLOR_SCALE);                                 //выводим текст "100"
//    tft.drawGFXText(88, 135, "200", COLOR_YELLOW);                                //выводим текст "200"
//    tft.drawGFXText(138,135, "300", COLOR_ORANGE);                                //выводим текст "300"
//    tft.drawGFXText(178,135, "400", COLOR_RED);}                                  //выводим текст "400"
//
//  if (Scale_hi==2){                                                               //если выбрана выходная шкала 2
//    tft.drawGFXText(6,  135, "0", COLOR_SCALE);                                   //выводим текст "0"
//    tft.drawGFXText(38, 135, "150", COLOR_SCALE);                                 //выводим текст "150"
//    tft.drawGFXText(88, 135, "300", COLOR_YELLOW);                                //выводим текст "300"
//    tft.drawGFXText(138,135, "450", COLOR_ORANGE);                                //выводим текст "450"
//    tft.drawGFXText(178,135, "600", COLOR_RED);}                                  //выводим текст "600"
//
//  if (Scale_hi==3){                                                               //если выбрана выходная шкала 3
//    tft.drawGFXText(6,  135, "0", COLOR_SCALE);                                   //выводим текст "0"
//    tft.drawGFXText(38, 135, "200", COLOR_SCALE);                                 //выводим текст "200"
//    tft.drawGFXText(88, 135, "400", COLOR_YELLOW);                                //выводим текст "400"
//    tft.drawGFXText(138,135, "600", COLOR_ORANGE);                                //выводим текст "600"
//    tft.drawGFXText(178,135, "800", COLOR_RED);}                                  //выводим текст "800"
//
//  if (Scale_hi==4){                                                               //если выбрана выходная шкала 4
//    tft.drawGFXText(6,  135, "0", COLOR_SCALE);                                   //выводим текст "0"
//    tft.drawGFXText(38, 135, "250", COLOR_SCALE);                                 //выводим текст "250"
//    tft.drawGFXText(88, 135, "500", COLOR_YELLOW);                                //выводим текст "500"
//    tft.drawGFXText(138,135, "750", COLOR_ORANGE);                                //выводим текст "750"
//    tft.drawGFXText(188,135, "1k", COLOR_RED);}}                                  //выводим текст "1k"
//
//    V=((U1*5)/1022)*k;                                                            //приводим напряжение к мощности 
//    PWR=(V*V)/50;                                                                 //вычисляем мощность
//  if (PWR>=PWRround*10){int PWR1=PWR/PWRstep; PWR=PWR1*PWRstep;}                  //свыше PWRround показания мощности приводим кратно PWRstep
//    dtostrf(PWR, len, 0, buf);                                                    //преобразуем значение PWR в массив buf
//    buf[len] = 'W'; buf[len+1] = 0;                                               //добавляем к массиву знак W
//    tft.drawText(153,8,buf+String (' '),COLOR_GREEN);}                            //выводим значение мощности


//********************************* Подготовка экрана ******************************************
//void start_LCD(){
//    tft.clear();                                                                  //очищаем экран
//    View_scale=EEPROM.read(1);                                                    //читаем данные с 1 ячейки памяти вида шкалы
//    Scale_low= EEPROM.read(2);                                                    //читаем данные с 2 ячейки памяти масштаб малой мощности
//    Scale_hi=  EEPROM.read(3);                                                    //читаем данные с 3 ячейки памяти масштаб большой мощности
//    EEPROM.get(4,SWRlow);                                                         //читаем данные с 4 по 7 ячейки памяти значение сигнализации SWRlow
//    EEPROM.get(8,SWRhi);                                                          //читаем данные с 8 по 11 ячейки памяти срабатывания сигнализации SWRhi
//    PWRround=  EEPROM.read(12);                                                   //читаем данные с 12 ячейки памяти предела мощности округления
//    PWRstep=   EEPROM.read(13);                                                   //читаем данные с 13 ячейки памяти шага округления мощности
//    Tone=      EEPROM.read(14);                                                   //читаем данные с 14 ячейки памяти значение тональности сигнала
//                                                                                  //коэф. мощности и пороги переключения для малой и большой мощности
//  if (Scale_low==0) {l=6.326; PWRthreshold=20;}                                   //малая мощность 20Вт, порог переключения 20Вт
//  if (Scale_low==1) {l=8.945; PWRthreshold=40;}                                   //малая мощность 40Вт, порог переключения 40Вт
//  if (Scale_low==2) {l=12.65; PWRthreshold=80;}                                   //малая мощность 80Вт, порог переключения 80Вт
//  if (Scale_low==3) {l=15.495; PWRthreshold=120;}                                 //малая мощность 120Вт, порог переключения 120Вт
//  if (Scale_hi==0){k=20;}                                                         //большая мощность 200Вт
//  if (Scale_hi==1){k=28.285;}                                                     //большая мощность 400Вт
//  if (Scale_hi==2){k=34.643;}                                                     //большая мощность 600Вт
//  if (Scale_hi==3){k=40;}                                                         //большая мощность 800Вт
//  if (Scale_hi==4){k=44.722;}                                                     //большая мощность 1000Вт
//  for (int i=0; i<2; i++){                                                        //включаем цикл
//    tft.drawRectangle(i, i, 219-i, 175-i, COLOR_DARKCYAN);                        //рисуем прямоугольник по внешнему контуру ЖКИ
//    tft.drawLine(10,66+i,207,66+i,COLOR_SCALE);                                   //рисуем гориз. линию верхней шкалы
//    tft.drawLine(10,111+i,207,111+i,COLOR_SCALE);                                 //рисуем гориз. линию нижней шкалы
//    tft.drawLine(0,27+i,219,27+i,COLOR_DARKCYAN);                                 //рисуем верхнюю горизонтальную линию
//    tft.drawLine(0,148+i,219,148+i,COLOR_DARKCYAN);                               //рисуем нижнюю горизонтальную линию
//    tft.fillRectangle(10+195*i,61,12+195*i,67,COLOR_SCALE);                       //рисуем большие риски (крайние) верхней шкалы
//    tft.fillRectangle(10+195*i,111,12+195*i,117,COLOR_SCALE);}                    //рисуем большие риски (крайние) нижней шкалы
//    
//  for (int i=0; i<19; i++){                                                       //включаем цикл
//  if ((i!=4)&&(i!=14))                                                            //кроме 5-й и 15-й
//    tft.fillRectangle(18+i*10,63,19+i*10,67,COLOR_SCALE);}                        //рисуем маленькие риски верхней шкалы
//    
//  for(int i=0; i<3; i++){
//    tft.drawLine(10,88+i,207,88+i,COLOR_SCALE);                                   //рисуем горизонтальные линии между шкалами
//    tft.fillRectangle(57+50*i,61,60+50*i,67,COLOR_SCALE);                         //рисуем большие риски верхней шкалы
//    tft.fillRectangle(55+50*i,111,57+50*i,117,COLOR_SCALE);}                      //рисуем большие риски нижней шкалы
//
//  for(int i=0; i<4; i++){                                                         //включаем цикл
//    tft.drawLine(31+50*i,111,31+50*i,116,COLOR_SCALE);}                           //рисуем маленькие риски нижней шкалы
//    
//    tft.drawGFXText(6,  55, "1",  COLOR_SCALE);                                   //выводим текст "1"
//    tft.drawGFXText(46, 55, "1.5",COLOR_SCALE);                                   //выводим текст "1.5"
//    tft.drawGFXText(104,55, "2",  COLOR_YELLOW);                                  //выводим текст "2"
//    tft.drawGFXText(146,55, "2.5",COLOR_ORANGE);                                  //выводим текст "2.5"
//    tft.drawGFXText(200,55, "3",  COLOR_RED);                                     //выводим текст "3"
//
//  if (Scale_low==0){                                                              //если выбрана входная шкала 1
//    tft.drawGFXText(6,  135, "0", COLOR_SCALE);                                   //выводим текст "0"
//    tft.drawGFXText(48, 135, "5", COLOR_SCALE);                                   //выводим текст "5"
//    tft.drawGFXText(94, 135, "10", COLOR_YELLOW);                                 //выводим текст "10"
//    tft.drawGFXText(144,135, "15", COLOR_ORANGE);                                 //выводим текст "15"
//    tft.drawGFXText(190,135, "20", COLOR_RED);}                                   //выводим текст "20"
//  
//  if (Scale_low==1){                                                              //если выбрана входная шкала 2
//    tft.drawGFXText(6,  135, "0", COLOR_SCALE);                                   //выводим текст "0"
//    tft.drawGFXText(44, 135, "10", COLOR_SCALE);                                  //выводим текст "10"
//    tft.drawGFXText(94, 135, "20", COLOR_YELLOW);                                 //выводим текст "20"
//    tft.drawGFXText(144,135, "30", COLOR_ORANGE);                                 //выводим текст "30"
//    tft.drawGFXText(190,135, "40", COLOR_RED);}                                   //выводим текст "40"
//
//  if (Scale_low==2){                                                              //если выбрана входная шкала 3
//    tft.drawGFXText(6,  135, "0", COLOR_SCALE);                                   //выводим текст "0"
//    tft.drawGFXText(44, 135, "20", COLOR_SCALE);                                  //выводим текст "20"
//    tft.drawGFXText(94, 135, "40", COLOR_YELLOW);                                 //выводим текст "40"
//    tft.drawGFXText(144,135, "60", COLOR_ORANGE);                                 //выводим текст "60"
//    tft.drawGFXText(190,135, "80", COLOR_RED);}                                   //выводим текст "80"
//
//  if (Scale_low==3){                                                              //если выбрана входная шкала 4
//    tft.drawGFXText(6,  135, "0", COLOR_SCALE);                                   //выводим текст "0"
//    tft.drawGFXText(44, 135, "30", COLOR_SCALE);                                  //выводим текст "40"
//    tft.drawGFXText(94, 135, "60", COLOR_YELLOW);                                 //выводим текст "60"
//    tft.drawGFXText(144,135, "90", COLOR_ORANGE);                                 //выводим текст "80"
//    tft.drawGFXText(178,135, "120", COLOR_RED);}                                  //выводим текст "120"
//    
//    tft.drawText(10,8,"SWR=",COLOR_SKYBLUE);                                      //выводим текст "SWR=", цвет голубой
//    tft.drawText(110,8,"PWR=",COLOR_SKYBLUE);                                     //выводим текст "PWR=", цвет голубой
//}

//**************************************** Обработчик прерываний ************************************
// Прерывания отрабатываются по сигналу PTT на любом из двух портов от трансиверов
// отрабатывается блокировка программы, в т.ч. любых переключений (кроме подключения усилителя в автоматическом режиме)
// и запускается управление выходом на трансвиеры - в зависомости от значения переменной trx_upr_mode
// Обязательна блокировка поступления прерывания от другого входа !!!!
void tx_on_block() {
  
}

//**************************************** Бипер ****************************************************
void beeper(){                                                                
//    tone(11,tone_beep,100);
    D11_Beep(100, tone_beep);  
}
void beeper_long(){                                                                
//    tone(11,tone_beep,500);
    D11_Beep(500, tone_beep);
}
void beeper_err(){
    tone(11,tone_beep,100);
    delay_ms(50);
    tone(11,tone_beep,100);
    delay_ms(50);
    tone(11,tone_beep,100);
}

//****************************** Подпрограмма меню *****************************************
//void menu() {                                                                       //подпрограмма меню настроек
//    #define TEXT COLOR_LIGHTCYAN                                                    //цвет текста меню
//    #define TEXT_SEL COLOR_RED                                                      //цвет выделенного пункта меню
//    int cx=8, cy=22, dx=cx+155;                                                     //начальные координаты меню
//    tft.clear(); tft.setBacklightBrightness(Bmax);                                  //очистка экрана, яркость подсветки Bmax
//  for (int i=0; i<2; i++){                                                          //включаем цикл
//    tft.drawRectangle(i, i, 219-i, 175-i, COLOR_DARKCYAN);}                         //рисуем прямоугольник по внешнему контуру ЖКИ
//
//    tft.drawGFXText(cx,cy,    "1. View scale",TEXT);                                //выводим текст "1. View scale"
//    tft.drawGFXText(cx,cy+20, "2. Scale low", TEXT);                                //выводим текст "2. Scale low"
//    tft.drawGFXText(cx,cy+40, "3. Scale hi",  TEXT);                                //выводим текст "3. Scale hi"
//    tft.drawGFXText(cx,cy+60, "4. SWR low",   TEXT);                                //выводим текст "4. SWR low"
//    tft.drawGFXText(cx,cy+80, "5. SWR hi",    TEXT);                                //выводим текст "5. SWR hi"
//    tft.drawGFXText(cx,cy+100,"6. PWR round", TEXT);                                //выводим текст "6. PWR round"
//    tft.drawGFXText(cx,cy+120,"7. PWR step",  TEXT);                                //выводим текст "7. PWR step"
//    tft.drawGFXText(cx,cy+140,"8. Tone beep", TEXT);                                //выводим текст "8. Tone beep"
//
//    int Select1,Select2,Select3,Select6,Select7,Select8;                            //переменные счетчиков
//    float Select4, Select5;                                                         //переменные счетчиков
//    int ExitMenu=0;                                                                 //
//    const char*view_scale[]={"1","2"};                                              //символьная константа
//    const char*scale_low[]={"20  ","40","80","120"};                                //символьная константа
//    const char*scale_hi[]={"200  ","400","600","800","1000"};                       //символьная константа
//    const char*pwrstep[]={"1  ","2","5","10","50",};                                //
//    const char*tonebeep[]={"OFF  ","100","200","300","400","500","600","700","800", //
//    "900","1000","1100","1200","1300","1400","1500","1600","1700","1800","1900",    //
//    "2000"};                                                                        //
//    char buf[5]; byte len=4;                                                        //символьный массив
//
//    Select1=EEPROM.read(1);                                                         //читаем значение View_scale и переносим его в счетчик Select1
//    Select2=EEPROM.read(2);                                                         //читаем значение Scale_low и переносим его в счетчик Select2
//    Select3=EEPROM.read(3);                                                         //читаем значение Scale_hi и переносим его в счетчик Select3
//    EEPROM.get(4,Select4);                                                          //читаем значение SWRlow и переносим его в счетчик Select4
//    EEPROM.get(8,Select5);                                                          //читаем значение SWRhi и переносим его в счетчик Select5
//    Select6=EEPROM.read(12);                                                        //читаем значение PWRround и переносим его в счетчик Select6
//    PWRstep=EEPROM.read(13);                                                        //читаем значение PWRstep и переносим его в счетчик Select7
//  if (PWRstep==1) Select7=0;                                                        //
//  if (PWRstep==2) Select7=1;                                                        //
//  if (PWRstep==5) Select7=2;                                                        //
//  if (PWRstep==10)Select7=3;                                                        //
//  if (PWRstep==50)Select7=4;                                                        //
//    Select8=EEPROM.read(14);                                                        //читаем значение Tone и переносим его в счетчик Select8
//
//    tft.drawText(dx,cy-12,view_scale[Select1],TEXT);                                //выводим значение Select1
//    tft.drawText(dx,cy+8,scale_low[Select2],TEXT);                                  //выводим значение Select2
//    tft.drawText(dx,cy+28,scale_hi[Select3],TEXT);                                  //выводим значение Select3
//    dtostrf(Select4,-len,1,buf); tft.drawText(dx,cy+48,buf,TEXT);                   //выводим значение Select4
//    dtostrf(Select5,-len,1,buf); tft.drawText(dx,cy+68,buf,TEXT);                   //выводим значение Select5
//    dtostrf(Select6*10,-len,0,buf); tft.drawText(dx,cy+88,buf+String (' '),TEXT);   //выводим значение Select6
//    tft.drawText(dx,cy+108,pwrstep[Select7],TEXT);                                  //выводим значение Select7
//    tft.drawText(dx,cy+128,tonebeep[Select8],TEXT);                                 //выводим значение Select8
//
//  do {                                                                              //включаем цикл опроса
//switch(button1.Loop()){                                                             //при нажатии кнопки 1
//  case SB_CLICK: beeper(); if (++position>8) position=1; break;                     //короткое нажатие,бипер, увеличиваем значение позиции на 1, ограничение до 8
//  case SB_LONG_CLICK: beeper(); ExitMenu=1; break;}                                 //длинное нажатие, бипер, выход из меню
//  
//  if (position==1){tft.drawGFXText(cx,cy, "1. View scale", TEXT_SEL);               //если позиция 1, выводим "1. View scale" красным цветом
//      tft.drawGFXText(cx,cy+140,"8. Tone beep", TEXT);                              //выводим "8. Tone beep" цветом TEXT
//switch(button2.Loop()){                                                             //если кнопка 2 нажата
//  case SB_CLICK: beeper(); if (++Select1>1) Select1=0; break;}                      //короткое нажатие, бипер, увеличиваем счетчик Select1 на 1, ограничиваем до 1
//    tft.drawText(dx,cy-12,view_scale[Select1],TEXT_SEL);}                           //выводим текст из символьной константы view_scale цветом TEXT_SEL
//  else tft.drawText(dx,cy-12,view_scale[Select1],TEXT);                             //иначе выводим текст из символьной константы view_scale цветом TEXT
//  
//  if (position==2){tft.drawGFXText(cx,cy+20, "2. Scale low", TEXT_SEL);             //если позиция 2, выводим "2. Scale low" красным цветом
//      tft.drawGFXText(cx,cy,    "1. View scale",TEXT);                              //выводим "1. View scale" цветом TEXT
//switch(button2.Loop()){                                                             //если кнопка 2 нажата
//  case SB_CLICK: beeper(); if (++Select2>3) Select2=0; break;}                      //короткое нажатие, бипер, увеличиваем счетчик Select2 на 1, ограничиваем до 3
//    tft.drawText(dx,cy+8,scale_low[Select2],TEXT_SEL);}                             //выводим текст из символьной константы scale_low цветом TEXT_SEL
//  else tft.drawText(dx,cy+8,scale_low[Select2],TEXT);                               //иначе выводим текст из символьной константы scale_low цветом TEXT
//  
//  if (position==3){tft.drawGFXText(cx,cy+40, "3. Scale hi", TEXT_SEL);              //если позиция 3, выводим "3. Scale hi" красным цветом
//      tft.drawGFXText(cx,cy+20, "2. Scale low", TEXT);                              //выводим "2. Scale low" цветом TEXT
//switch(button2.Loop()){                                                             //если кнопка 2 нажата
//  case SB_CLICK: beeper(); if (++Select3>4) Select3=0; break;}                      //короткое нажатие, бипер, увеличиваем счетчик Select3 на 1, ограничиваем до 4
//    tft.drawText(dx,cy+28,scale_hi[Select3],TEXT_SEL);}                             //выводим текст из символьной константы scale_hi цветом TEXT_SEL
//  else tft.drawText(dx,cy+28,scale_hi[Select3],TEXT);                               //иначе выводим текст из символьной константы scale_hi цветом TEXT
//  
//  if (position==4){tft.drawGFXText(cx,cy+60, "4. SWR low", TEXT_SEL);               //если позиция 4, выводим "4. SWR low" красным цветом
//      tft.drawGFXText(cx,cy+40, "3. Scale hi", TEXT);                               //выводим "3. Scale hi" цветом TEXT
//switch(button2.Loop()){                                                             //если кнопка 2 нажата
//  case SB_CLICK: beeper(); Select4=Select4+0.1; if(Select4>3.0){Select4=1.0;}break;}//короткое нажатие, бипер, увеличиваем счетчик Select4 на 0.1, ограничиваем до 1.0 ... 3.0
//  if (Select4>Select5) Select4=Select5;                                             //
//    dtostrf(Select4,-len,1,buf); tft.drawText(dx,cy+48,buf,TEXT_SEL);}              //преобразуем значение счетчика Select4 в текст, выводим значение на экран цветом TEXT_SEL
//  else {dtostrf(Select4,-len,1,buf); tft.drawText(dx,cy+48,buf,TEXT);}              //иначе преобразуем значение счетчика Select4 в текст, выводим значение на экран цветом TEXT
//  
//  if (position==5){tft.drawGFXText(cx,cy+80, "5. SWR hi", TEXT_SEL);                //если позиция 5, выводим "5. SWR hi" красным цветом
//      tft.drawGFXText(cx,cy+60, "4. SWR low", TEXT);                                //выводим "4. SWR low" цветом TEXT
//switch(button2.Loop()){                                                             //если кнопка 2 нажата
//  case SB_CLICK: beeper(); Select5=Select5+0.1; if(Select5>3.0){Select5=1.5;}break;}//короткое нажатие, бипер, увеличиваем счетчик Select5 на 0.1, ограничиваем до 1.5 ... 3.0
//    dtostrf(Select5,-len,1,buf); tft.drawText(dx,cy+68,buf,TEXT_SEL);}              //преобразуем значение счетчика Select5 в текст, выводим значение на экран цветом TEXT_SEL
//  else {dtostrf(Select5,-len,1,buf); tft.drawText(dx,cy+68,buf,TEXT);}              //иначе преобразуем значение счетчика Select5 в текст, выводим значение на экран цветом TEXT
//  
//  if (position==6){tft.drawGFXText(cx,cy+100,"6. PWR round", TEXT_SEL);             //если позиция 6, выводим "6. PWR round" красным цветом
//      tft.drawGFXText(cx,cy+80, "5. SWR hi", TEXT);                                 //выводим "5. SWR hi" цветом TEXT
//switch(button2.Loop()){                                                             //если кнопка 2 нажата
//  case SB_CLICK: beeper(); Select6=Select6+10; if(Select6>100){Select6=10;} break;} //короткое нажатие, бипер, увеличиваем счетчик Select6 на 10, ограничиваем до 10 ... 100
//    dtostrf(Select6*10,-len,0,buf);                                                 //преобразуем значение счетчика Select6 в текст
//    tft.drawText(dx,cy+88,buf+String (' '),TEXT_SEL);}                              //выводим значение на экран цветом TEXT_SEL
//  else {dtostrf(Select6*10,-len,0,buf);                                             //иначе преобразуем значение счетчика Select6 в текст
//    tft.drawText(dx,cy+88,buf+String (' '),TEXT);}                                  //и выводим значение на экран цветом TEXT
//  
//  if (position==7){tft.drawGFXText(cx,cy+120,"7. PWR step", TEXT_SEL);              //если позиция 7, выводим "7. PWR step" красным цветом
//      tft.drawGFXText(cx,cy+100,"6. PWR round", TEXT);                              //выводим "6. PWR round" цветом TEXT
//switch(button2.Loop()){                                                             //если кнопка 2 нажата
//  case SB_CLICK: beeper(); if (++Select7>4){Select7=0;}                             //короткое нажатие, бипер, увеличиваем счетчик Select7 на 1, ограничиваем до 1 ... 10
//  if (Select7==0)PWRstep=1;                                                         //
//  if (Select7==1)PWRstep=2;                                                         //
//  if (Select7==2)PWRstep=5;                                                         //
//  if (Select7==3)PWRstep=10;                                                        //
//  if (Select7==4)PWRstep=50; break;}                                                //
//    tft.drawText(dx,cy+108,pwrstep[Select7],TEXT_SEL);}                             //выводим значение на экран цветом TEXT_SEL
//  else {tft.drawText(dx,cy+108,pwrstep[Select7],TEXT);}                             //иначе выводим значение на экран цветом TEXT
//  
//  if (position==8){tft.drawGFXText(cx,cy+140,"8. Tone beep", TEXT_SEL);             //если позиция 8, выводим "8. Tone beep" красным цветом
//      tft.drawGFXText(cx,cy+120,"7. PWR step", TEXT);                               //выводим "7. PWR step" цветом TEXT
//switch(button2.Loop()){                                                             //если кнопка 2 нажата
//  case SB_CLICK: if (++Select8>20){Select8=0;}                                      //короткое нажатие, увеличиваем счетчик Select8 на 10, ограничиваем до 100 ... 250
//    Tone=Select8; if (Tone!=0) beeper(); break;                                     //переносим новое значение счетчика Select8 в Tone, бипер
//  case SB_AUTO_CLICK: if (++Select8>20){Select8=0;}                                 //
//    Tone=Select8; if (Tone!=0) beeper(); break;}                                    //
//    tft.drawText(dx,cy+128,tonebeep[Select8],TEXT_SEL);}                            //преобразуем значение счетчика Select8 в текст, выводим значение на экран цветом TEXT_SEL
//  else tft.drawText(dx,cy+128,tonebeep[Select8],TEXT);                              //иначе преобразуем значение счетчика Select8 в текст, выводим значение на экран цветом TEXT
//  if (Select4>Select5) Select4=Select5;                                             //
//
//} while (ExitMenu==0);                                                              //выполняем цикл, когда счетчик позиций меньше 9
//
//EEPROM.update(1,Select1); EEPROM.update(2,Select2); EEPROM.update(3,Select3);       //заносим новые значение в ЕЕПРОМ, если они были изменены
//EEPROM.put(4,Select4);    EEPROM.put(8,Select5);    EEPROM.update(12,Select6);      //-------------------------------------------------------
//EEPROM.update(13,PWRstep);EEPROM.update(14,Select8);                                //-------------------------------------------------------
//    val_clock=0; val_power=0; flag_power=0; flag_scale=1;                           //обнуляем счетчик val_clock, флаги val_power и flag_power, флаг шкалы в 1
//  if (flag_clock==1) start_LCD();                                                   //если флаг flag_clock=1, запускаем подпрограмму start_LCD
//  if (flag_clock==0) flag_clock=1;                                                  //если флаг flag_clock=0, изменяем его значение на 1
//}//*****************************************************************************************
