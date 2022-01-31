/**
 * Работа с кнопками
 * Copyright (C) 2016 Alexey Shikharbeev
 * http://samopal.pro
*/
#include "sav_button.h"
SButton button1(4,50,2000,4000,1000);
SButton button2(5,50,2000,4000,1000);

void setup()
{
   Serial.begin(115200);
   Serial.println("Test smart button ...");
    
// Инициация кнопок 
   button1.begin();
   button2.begin();
}

void loop(){
   switch( button1.Loop() ){
      case SB_CLICK:
         Serial.println("Press button 1");
         break;
      case SB_LONG_CLICK:
         Serial.println("Long press button 1");
         break;
      case SB_AUTO_CLICK:
         Serial.println("Auto press button 1");
         break;       
   }
   switch( button2.Loop() ){
      case SB_CLICK:
         Serial.println("Press button 2");
         break;
      case SB_LONG_CLICK:
         Serial.println("Long press button 2");
         break;
      case SB_AUTO_CLICK:
         Serial.println("Auto press button 2");
         break;       
   }


  
}



