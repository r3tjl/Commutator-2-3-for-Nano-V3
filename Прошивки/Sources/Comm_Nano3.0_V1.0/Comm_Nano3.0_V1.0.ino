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
#define rel_9 13
//*************Определяем имена прочих пинов входов и выходов***********************
#define ptt1 2
#define ptt2 3
#define tx1 14
#define tx2 15
#define mode_key A2
#define ant_key A3

#define tone_beep 1000                                                              //тональность бипера (1000-1кгц)


//************************* Назначаем переменные ***********************************
//  int Trc1, Trc2 = 0;                                                             //переменная состояния линий Трансивер1 и Трансивер2. 0 - не подкл. 1 - к А1. 2 - к А2. 3 - к А3
  byte ant1, ant2, ant3 = 0;                                                        //переменные состояния антенных входов: 0 - не подкл., 1 - к Тр1, 2 - к Тр2
  byte paLine = 0;                                                                   //переменная подключения усилителя 0 - не подкл. 1 - к Тр1. 2 - к Тр2
  byte trxUprMode;                            //переменная режима управления передачей трансиверов: 0-без упр., 1-только блок., 2-упр.по текущему выбору, 3-полное управление
                                                         //переменная используется в прерываниях
  
  boolean tr1Tx = false;                         //флаги блокировки трансиверов (ПОД ВОПРОСОМ - возможно будут означать наоборот передачу
  boolean tr2Tx = false;
  
  byte actTrx1 = 1;                               //переменная показывает что активным для действий является трансивер 1. Если false - то трансивер 2
  boolean blockMode = true;                             //переменная определяет что включен режим блокировки коммутции 
  boolean antBlockTx = false;                           //переменная определяет включен ли режим блокировки других антенн при передаче
  boolean onCommut = false;                             //флаг необходимости перекоммутации. Если true - выполняется блок коммутации
  boolean autoPa = false;                               //флаг автоматчиеского подключения усилителя к передающему каналу
  boolean onInterrupt = false;                         //флаг наличия необработанного прерывания по любому входу

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
  pinMode(rel_9,OUTPUT);

  pinMode(ptt1,INPUT);
  pinMode(ptt2,INPUT);

  pinMode(tx1,OUTPUT);
  pinMode(tx2,OUTPUT);

  pinMode(mode_key,INPUT);     //надо ли так конфигурировать вход для аналогового?
  pinMode(ant_key,INPUT);

//**************прерывания на входах 2 и 3
attachInterrupt(0, tx_interrupt, LOW);                 // возможно надо ПО ФРОНТУ - с высокого уровня на низкий!
attachInterrupt(1, tx_interrupt, LOW);
  

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
  lcd.clear();
  lcd.print(" AutoAnt.Switch");
  lcd.setCursor(2, 1);
  lcd.print("v1.0 by R3TJL");
  delay_ms (2000);

trxUprMode=EEPROM.read(1);
if (trxUprMode < 1 || trxUprMode > 3) {
  lcd.clear();
  lcd.print("Please set Trx");
  lcd.setCursor(2,1); 
  lcd.print("control Mode");
  beeper_err();
  delay_ms(3000);
  trxUprMode = 0; 
  EEPROM.update(1,0);
}
start_LCD();

}
//=============================== Основной цикл программы =====================================
void loop() {
  Start

//***************************БЛОК КОММУТАЦИИ (выполняется по флагу необходимости коммутации**********************
if (onCommut) {

  if (ant1 != 0) digitalWrite(rel_1, HIGH); else digitalWrite(rel_1,LOW);
  if (ant2 != 0) digitalWrite(rel_2, HIGH); else digitalWrite(rel_2,LOW);
  if (ant3 != 0) digitalWrite(rel_3, HIGH); else digitalWrite(rel_3,LOW);

  if (paLine == 1) {                    //если УМ подключается к Тр1
    if (!autoPa) digitalWrite(rel_8, HIGH); else digitalWrite(rel_8, LOW);
    digitalWrite(rel_7, HIGH);
    if (ant1 == 1) digitalWrite(rel_4, HIGH);
    if (ant2 == 1) digitalWrite(rel_5, HIGH);
    if (ant3 == 1) digitalWrite(rel_6, HIGH);
    
    if (ant1 == 2) digitalWrite(rel_4, LOW);
    if (ant2 == 2) digitalWrite(rel_5, LOW);
    if (ant3 == 2) digitalWrite(rel_6, LOW);
  }
  if (paLine == 2 || paLine == 0) {                    //если УМ подключается к Тр2
    if (paLine == 2 && !autoPa) digitalWrite(rel_8, HIGH); else digitalWrite(rel_8, LOW);
    digitalWrite(rel_7, LOW);
    if (ant1 == 1) digitalWrite(rel_4, LOW);
    if (ant2 == 1) digitalWrite(rel_5, LOW);
    if (ant3 == 1) digitalWrite(rel_6, LOW);
    
    if (ant1 == 2) digitalWrite(rel_4, HIGH);
    if (ant2 == 2) digitalWrite(rel_5, HIGH);
    if (ant3 == 2) digitalWrite(rel_6, HIGH);
  }  
  // при любых установленных paLine если флаг автоподкл.УМ autoPa = истина, то у усилителя включен обход (реле 8 - LOW)
  // при авоматическом подключении УМ наджо делать доп. включение реле 8 перед передачей, но после всей остальной коммутации
  // Эту обработку надо делать после блока коммутации сразу !!! То есть при авто режиме усилителя при каждом переходе на передачу 
  // должен срабатывать сначала блок коммутации, а потом ВРЕМЕННО на время передачи включаться реле 8 и запускаться передача на 
  // соответствующем выходе.


  
  //задать функции блокировки !!!! или можно их задать в логическом блоке, а коммутацию здесь
  
  // это просто принудительная блокировка передачи любого трансивера если он не подключен к антенне НИКАК. Можно сделать и в логическом
  if (ant1 != 1 && ant2 != 1 && ant3 != 1 && trxUprMode != 0) tr1Tx = false; digitalWrite(tx1, LOW);     //нужна ли здесь проверка trxUprMode != 0  ???
  if (ant1 != 2 && ant2 != 2 && ant3 != 2 && trxUprMode != 0) tr2Tx = false; digitalWrite(tx2, LOW);       // флаги tr1Tx, tr2Tx


  
  onCommut = false;                   //сброс флага коммутации - коммутация заверщена
}
//***************************конец БЛОКА КОММУТАЦИИ**************************************************************
  
//***************************БЛОК ИНДИКАЦИИ***********************************************************************

if (actTrx1 == 1) {
  lcd.setCursor(0, 1); lcd.print(" ");
  lcd.setCursor(0, 0); lcd.print(">");
}
else {
  lcd.setCursor(0, 0); lcd.print(" ");
  lcd.setCursor(0, 1); lcd.print(">");
}

lcd.setCursor(4, 0); lcd.print("--");
lcd.setCursor(4, 1); lcd.print("--");

switch(ant1) {
  case 1:
    lcd.setCursor(4, 0); lcd.print("A1");
    break;
  case 2:
    lcd.setCursor(4, 2); lcd.print("A1");
    break;
}
switch(ant2) {
  case 1:
    lcd.setCursor(4, 0); lcd.print("A2");
    break;
  case 2:
    lcd.setCursor(4, 2); lcd.print("A2");
    break;
}
switch(ant3) {
  case 1:
    lcd.setCursor(4, 0); lcd.print("A3");
    break;
  case 2:
    lcd.setCursor(4, 2); lcd.print("A3");
    break;
}

lcd.setCursor(7, 0); lcd.print("--");
lcd.setCursor(7, 1); lcd.print("--");
switch (paLine) {
  case 1:
    lcd.setCursor(7, 0); lcd.print("PA");
    break;
}
if (autoPa & paLine != 0) lcd.print("Au");
else if (paLine != 0) lcd.print("PA");

lcd.setCursor(14,0);
if (blockMode) lcd.print("BL");
else lcd.print("sw");

lcd.setCursor(14,1);
if (antBlockTx) lcd.print("BL");
else lcd.print("on");

//***************************конец БЛОКА ИНДИКАЦИИ****************************************************************

//****************************БЛОК опроса кнопок***********************************
// НЕЛЬЗЯ ВЫНОСИТЬ В ФУНКЦИЮ !!!! Переменные определяем как глобальные? 

//считываем значение от кнопок выбора антенного входа (только короткие)
  value = find_similar(analogRead(ant_key), 10, 100);   //вопрос - работает ли эта функция от дребезга библиотеки CyberLib.h ???
                                                        //иначе - простое считывание analogRead(ant_key)
    if (value > 900) {
      key_ant = 0;}
    if (value < 100 && last_a > 900) { 
      key_ant = 3;              //*** нажата кнопка А3
      beeper();}
    if (value > 100 && value < 400 && last_a > 900) { 
      key_ant = 2;              //*** нажата кнопка А2
      beeper();}
    if (value >400 && value < 900 && last_a > 900) { 
      key_ant = 1;              //*** нажата кнопка А1
      beeper();}
    last_a = value;

//считываем значения от кнопок выбора режима (короткие и длинные)
  value = find_similar(analogRead(mode_key), 10, 100);
    if (value > 900 && press_flag == false && long_press_flag == false) {
      key_mode = 0;    }
    //если нажата кнопка 3 (УМ)
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
      autoPa = !autoPa;           //меняем состяние режима автоподключения УМ
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

    //если нажата кнопка 2 (TRx)
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
      blockMode = !blockMode;     //меняем переменную, циклично переключаем режим коммутации: блокировка <-> переключение. Можно вынести в логический блок
      beeper_long();
    }
    else if (value > 900 && last_value > 100 && last_value < 400 && press_flag == true && long_press_flag == false) {
      // если кнопка отпущена и было только короткое нажатие, то...
      press_flag = !press_flag;  // опустить флажок короткого нажатия
      last_press = 0;
      last_value = 1023;
      // сюда вписываем события неоходимые при коротком нажатии на кнопку
      key_mode = 2; 
      if (actTrx1 == 1) actTrx1 = 2;         //меняем переменную - циклично выбираем активнвм трансивер 1 или 2. Можно вынести в логический блок
      else actTrx1 = 1;
      beeper();
    }
      //если нажата кнопка 1 (Реж.)
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
      antBlockTx = !antBlockTx;  //меняем режим блокировки прочих антенн при передаче
      beeper();
    }
//************************конец блока опроса кнопок управления *****************************************


//************************Блок логики и безопасности ***************************************************


//*************логика включения антенных входов ***************
switch (ant_key) {
  case 1:
    if (actTrx1 == ant1) ant1 = 0; onCommut = true;           //нажатие на кнопку уже подключенной антенны - ее отключение
    
    if (actTrx1 == 1 && ant1 == 2) { 
      if (blockMode) beeper_err();
      else ant1 = 1; onCommut = true;
    }
    if (actTrx1 == 2 && ant1 == 1) {
      if (blockMode) beeper_err();
      else ant1 = 2; onCommut = true;
    break;
    }
   case 2:
    if (actTrx1 == ant2) ant2 = 0; onCommut = true;          //нажатие на кнопку уже подключенной антенны - ее отключение
    
    if (actTrx1 == 1 && ant2 == 2) { 
      if (blockMode) beeper_err();
      else ant2 = 1; onCommut = true;
    }
    if (actTrx1 == 2 && ant2 == 1) {
      if (blockMode) beeper_err();
      else ant2 = 2; onCommut = true;
    break;
    }    
   case 3:
    if (actTrx1 == ant3) ant3 = 0; onCommut = true;           //нажатие на кнопку уже подключенной антенны - ее отключение
    
    if (actTrx1 == 1 && ant3 == 2) { 
      if (blockMode) beeper_err();
      else ant3 = 1; onCommut = true;
    break;
    }
    if (actTrx1 == 2 && ant3 == 1) {
      if (blockMode) beeper_err();
      else ant3 = 2; onCommut = true;
    }    
}

//**отладочный модуль, исключает одинаковое включение антенн. В дальнейшем если логика не позволит так включить - можно убрать 3 аварийных сигнала
if ((ant1 != 0 && ant1 == ant2) || (ant3 != 0 && ant1 == ant3) || (ant2 != 0 && ant2 == ant3)) {
  beeper_err(); beeper_err(); beeper_err();
  ant1, ant2, ant3 = 0; onCommut = true;
}
//*******************

//*********************************логика подключения УМ ***********************************

if (key_mode == 3) {
  if (actTrx1 == paLine) paLine = 0; onCommut = true;           //нажатие на кнопку когда ум ПОДКЛЮЧЕН - отключение

  if (actTrx1 == 1 && paLine == 2) { 
      if (blockMode) beeper_err();
      else paLine = 1; onCommut = true;
    } 
  if (actTrx1 == 2 && paLine == 1) { 
      if (blockMode) beeper_err();
      else paLine = 2; onCommut = true;
    } 
}

if (key_mode == 4) modeTx();


//******************************модуль блокировок***********************************


  //задать функции блокировки !!!! или можно их задать в логическом блоке, а коммутацию здесь
  
  // это просто принудительная блокировка передачи любого трансивера если он не подключен к антенне НИКАК. Можно сделать и в логическом
  if (ant1 != 1 && ant2 != 1 && ant3 != 1 && trxUprMode != 0) tr1Tx = false; digitalWrite(tx1, LOW);     //нужна ли здесь проверка trxUprMode != 0  ???
  if (ant1 != 2 && ant2 != 2 && ant3 != 2 && trxUprMode != 0) tr2Tx = false; digitalWrite(tx2, LOW);       // флаги tr1Tx, tr2Tx




//************************КОнец логического блока *****************************************************

End
}
//===============================================================================================


//********************************* Подпрограмма Подготовка рабочего экрана ******************************************
void start_LCD(){
  lcd.clear();
  lcd.print(" T1 -- -- Mod:");
  lcd.setCursor(0, 1);
  lcd.print(" T2 -- -- RxA:");
  lcd.setCursor(0, 3); lcd.print(char(126)); 
  lcd.setCursor(0, 6); lcd.print(char(127));
  lcd.setCursor(1, 3); lcd.print(char(126)); 
  lcd.setCursor(1, 6); lcd.print(char(127));
}
//******************************************************************************

//**************************************** Обработчик прерываний ************************************
// Прерывания отрабатываются по сигналу PTT на любом из двух портов от трансиверов
// отрабатывается блокировка программы, в т.ч. любых переключений (кроме подключения усилителя в автоматическом режиме)
// и запускается управление выходом на трансвиеры - в зависомости от значения переменной trx_upr_mode
// Обязательна блокировка поступления прерывания от другого входа !!!!
void tx_interrupt() {
  onInterrupt = true;

      
}


void tx_on_block() {
  detachInterrupt(0); detachInterrupt(1);           //запрещаем все прерывания до выхода из обработчика
  while (1) {
    
  }







  
  onInterrupt = false;                                                          //сброс флага наличия прерывания
  attachInterrupt(0, tx_interrupt, LOW); attachInterrupt(1, tx_interrupt, LOW); //разрешаем прерывания (последняя команда)
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

//****************************** подпрограмма режимов управления передачей *****************************************
void modeTx() {                                                           
  lcd.clear();
  lcd.print(" Trx control");
  lcd.setCursor(1, 1);
  lcd.print("MODE:");
  lcd.setCursor(7,1);
    switch (trxUprMode) {
    case 0:
      lcd.print("No(test)");
      break;
    case 1:
      lcd.print("BLCKonly");
      break;
    case 2:
      lcd.print("SelActTX");
      break;
    case 3:
      lcd.print("2-autoTX");
      break;
  }
  ++trxUprMode;
  if (trxUprMode > 3) trxUprMode = 0;             //или = 1 чтобы вообще убрать тестовый режим
  delay(100);                   //подбираем задержки
  lcd.setCursor(7,1);
    switch (trxUprMode) {
    case 0:
      lcd.print("No(test)");
      break;
    case 1:
      lcd.print("BLK only");
      break;
    case 2:
      lcd.print("SelActTX");
      break;
    case 3:
      lcd.print("2-autoTX");
      break;
  }
  EEPROM.update(1,trxUprMode);
  delay(900);                   //подбираем задержки
  start_LCD();
}
//*****************************************************************************************
