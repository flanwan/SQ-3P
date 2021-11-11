#include <EEPROM.h>
#include <LiquidCrystal.h>
LiquidCrystal lcd(2, 3, 4, 5, 6, 7);
int i;

void setup() {
  // put your setup code here, to run once:
   lcd.begin(16, 2);
   lcd.setCursor(0, 0); 
   lcd.print("Reset EEprom");
   /* */
   for (i=0;i<255;i++) {
       EEPROM.write(i,255);
   } 

   
   EEPROM.write(32,60); //C
   EEPROM.write(36,62); //D
   EEPROM.write(40,64); //E
   EEPROM.write(44,65); //F
   EEPROM.write(48,67); //G
   EEPROM.write(52,69); //A
   EEPROM.write(56,71); //H
   EEPROM.write(60,72); //C
   EEPROM.write(31,8); //Laststep
  // */
}

void loop() {
  // put your main code here, to run repeatedly:
   lcd.setCursor(0, 1);
   lcd.print("Reset Done");
}
