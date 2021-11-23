/* 
 *  cleaned variables
 *  added factory reset
 *  added potentiometer trimming
 */


 
#include <EEPROM.h>
#include <MIDI.h>
#include <LiquidCrystal.h>#
MIDI_CREATE_DEFAULT_INSTANCE();
LiquidCrystal lcd(2,3,4,5,6,7);

// Define inputs and outputs
const int button1Pin=8;   //PLAY
const int button2Pin=9;   //STOP
const int button3Pin=12;  //WRITE
const int button4Pin=11;  //F1
const int button5Pin=10;  //F2
const int clockInPin=13;  //CLOCK-IN

const int tempoCVInPin = A0; // =Tempo potentiometer
const int CV1Pin = A1; // (5V or CV1-In) via CV1 potentiometer
const int CV2Pin = A2; // (5V or CV2-In) via CV2 potentiometer
const int clockOutPin = A5; //->sent via the clock-socket-switch to clockInPin

//Offset variables for the CV ins, for adopting to potentiometer variations
int maxOffTempoCV;
int maxOffCV1;
int maxOffCV2;

// Variables to handle the potentiometers
unsigned long CV1;
unsigned long CV2;
unsigned long TempoCV;

// Variables to handle the buttons and digital inputs
int button1State=0; 
int button2State=0; 
int button3State=0;
int button4State=0;
int button5State=0;
int triggerState=0;
int button1StatePrev=0;
int button2StatePrev=0;
int button3StatePrev=0;
int button4StatePrev=0;
int button5StatePrev=0;
int clockInStatePrev=0;

// Variable required for the clock generator
float BPMf=120;
int BPMi=0;
unsigned long tempo=0;
unsigned long tempoX=0;
unsigned long clocktime=500;
int clockOn=0;
int clockOff=1;

unsigned long thistime;
unsigned long triggertime;
unsigned long lasttriggertime;


// Variables to handle the generic status of the software
int menu=0;
int edit=0;
int runmode=0; // 0=Stop, 1=Play
int writemode=0; // 1=Record
int maxmenu;
int adjuststep=0; //used for the poti adjustment procedure

// Variables for setup parameters
int outchannel=1;
int inchannel=1;
int notelength=1;
int transposemode=3;
int noteoffmode=1;
int notethrumode=3;
int controlthrumode=1;
int programthrumode=0;
int bendthrumode=1;
int otherthrumode=0;


// Variables for handling MIDI data
const int maxSteps=64;
const int maxNotes=6;
uint8_t note[maxSteps][maxNotes];
uint8_t noteOff[maxNotes];
uint8_t velocity;
uint8_t chdata;

// Variables for sequence handling
int lastStep=0;
int stepNumber=0;
int stepBack=0;
int stepLCDposition;
int noteInChord=0;
int keysPressed=0;
int maxNoteInChord=0;
int prevMaxNoteInChord=0;
int pause=0; // 0=normal play, 1=steps are counted, but no midi is sent
int waitEndOfPause=0;
int transpose=0;
int ontrigger=0;
int offtrigger=0;



void setup() {
   lcd.begin(16, 2);
   
   if (EEPROM.read(1) == 255) factoryreset(); 

   maxOffTempoCV=EEPROM.read(1);
   maxOffCV1=EEPROM.read(2);
   maxOffCV2=EEPROM.read(3);
   outchannel=EEPROM.read(4);
   inchannel=EEPROM.read(5);

   transposemode=EEPROM.read(8);
   noteoffmode=EEPROM.read(9);
   notelength=EEPROM.read(10);
   notethrumode=EEPROM.read(11);
   controlthrumode=EEPROM.read(12);
   programthrumode=EEPROM.read(13);
   bendthrumode=EEPROM.read(14);
   otherthrumode=EEPROM.read(15);
   
   pinMode(button1Pin, INPUT);
   pinMode(button2Pin, INPUT);
   pinMode(button3Pin, INPUT);
   pinMode(button4Pin, INPUT);
   pinMode(button5Pin, INPUT);
   pinMode(clockInPin, INPUT);
   pinMode(tempoCVInPin, INPUT);
   pinMode(CV1Pin, INPUT);
   pinMode(CV2Pin, INPUT);
   pinMode(clockOutPin, OUTPUT);

   MIDI.begin();
   MIDI.setThruFilterMode(1);
   MIDI.turnThruOff();
   velocity=96;

   midireset();


   /* FIRMWARE VERSION */
   lcd.setCursor(0, 0); 
   lcd.print(F("SQ-3P v1.10"));
   delay(1000);
   lcd.setCursor(0, 0);
   lcd.print(F("           "));  
   loadsequence();
      
   clearscreen();
        
}

void midireset() {
   int i=0;
   for (i=0; i <= 126 ; i++) {
      MIDI.sendNoteOff(i,0,outchannel);
      delay(1);
   }
   MIDI.sendControlChange(123,0,outchannel);
}

void printstep (int step, char type) {
  if (menu == 0) {
   if (step == 0) {
      lcd.setCursor(step,1);
      lcd.print(F("                "));
   }
   lcd.setCursor(step,1);
   lcd.print(type);
  }
}

void clearscreen(){
   lcd.setCursor(0,0);
   lcd.print(F("                "));
   lcd.setCursor(0,1);
   lcd.print(F("                "));
//   delay(1);
}


void showtranspose(int transpose) {
   lcd.setCursor(4, 0);
   if (transpose < 0) {
      lcd.print(F(" Trns"));    
      lcd.setCursor(9, 0);
      lcd.print(transpose);
      if (transpose > -10) {
         lcd.setCursor(11, 0);
         lcd.print(F(" "));                 
      }
   }
   if (transpose == 0) lcd.print(F("        "));
   if (transpose > 0) {
      lcd.print(F(" Trns+"));    
      lcd.setCursor(10, 0);
      lcd.print(transpose);
      if (transpose < 10) {
         lcd.setCursor(11, 0);
         lcd.print(F(" "));                 
      }
   }
}


void adjustpotis() {
   unsigned long CV1raw=analogRead(CV1Pin) ;
   unsigned long CV2raw=analogRead(CV2Pin) ;
   unsigned long TempoCVraw=analogRead(tempoCVInPin) ;

      if (adjuststep == 0) {
         long tempoCV = (TempoCVraw * (1000.0 + maxOffTempoCV) / 1000.0) ;
         lcd.setCursor(0, 0);
         lcd.print(F("Tempo: "));
         lcd.print(TempoCVraw);
         lcd.print(F(" "));
         lcd.setCursor(1, 1);
         lcd.print(F("1023="));
         lcd.print(tempoCV);
         lcd.print(F(" "));
         lcd.print(maxOffTempoCV);
         lcd.print(F("        "));

         if (button4State == 1 && button4StatePrev == 0)  {
            maxOffTempoCV--;
            button4StatePrev=1;
         }
         if (button5State == 1 && button5StatePrev == 0 ) {
            maxOffTempoCV++;
            button5StatePrev=1;
         }
         if (button4State == 0 && button4StatePrev == 1) button4StatePrev=0;
         if (button5State == 0 && button5StatePrev == 1) button5StatePrev=0;

         if (tempoCV == 1023 ) {
            clearscreen();
            lcd.setCursor(0, 0);
            lcd.print(F("Tempo is good!"));
            EEPROM.write(1,maxOffTempoCV);

            delay(1000);
            clearscreen();
            adjuststep=1;
         }
       }
         

       if (adjuststep == 1) {
         CV1 = (CV1raw * (1000.0 + maxOffCV1) / 1000.0) ;
         lcd.setCursor(0, 0);
         lcd.print(F("CV1raw: "));
         lcd.print(CV1raw);
         lcd.print(F(" "));
         lcd.setCursor(1, 1);
         lcd.print(F("1023="));
         lcd.print(CV1);
         lcd.print(F(" "));
         lcd.print(maxOffCV1);
         lcd.print(F("        "));
         
         if (button4State == 1 && button4StatePrev == 0)  {
            maxOffCV1--;
            button4StatePrev=1;
         }
         if (button5State == 1 && button5StatePrev == 0 ) {
            maxOffCV1++;
            button5StatePrev=1;
         }
         if (button4State == 0 && button4StatePrev == 1) button4StatePrev=0;
         if (button5State == 0 && button5StatePrev == 1) button5StatePrev=0;
         
         if (CV1 == 1023 ) {
            clearscreen();
            lcd.setCursor(0, 0);
            lcd.print(F("CV1 is good!"));
            EEPROM.write(2,maxOffCV1);
            delay(1000);
            clearscreen();
            adjuststep=2;
         }
       } 

       if (adjuststep == 2) {
         CV2 = (CV2raw * (1000.0 + maxOffCV2) / 1000.0) ;
         lcd.setCursor(0, 0);
         lcd.print(F("CV2raw: "));
         lcd.print(CV2raw);
         lcd.print(F(" "));
         lcd.setCursor(1, 1);
         lcd.print(F("1023="));
         lcd.print(CV2);
         lcd.print(F(" "));
         lcd.print(maxOffCV2);
         lcd.print(F("        "));

         if (button4State == 1 && button4StatePrev == 0)  {
            maxOffCV2--;
            button4StatePrev=1;
         }
         if (button5State == 1 && button5StatePrev == 0 ) {
            maxOffCV2++;
            button5StatePrev=1;
         }
         if (button4State == 0 && button4StatePrev == 1) button4StatePrev=0;
         if (button5State == 0 && button5StatePrev == 1) button5StatePrev=0;
         
         if (CV2 == 1023 ) {
            clearscreen();
            lcd.setCursor(0, 0);
            lcd.print(F("CV2 is good!"));
            EEPROM.write(3,maxOffCV2);
            delay(1000);
            clearscreen();
            adjuststep=0;
            edit=0;
         }
       } 
}

void factoryreset() {
   clearscreen();
   lcd.setCursor(0, 0); 
   lcd.print(F("Reset EEprom"));
   int i=0;
   for (i=0;i<31;i++) {
       EEPROM.write(i,255);
   } 
   i=0;
   for (i=31;i<255;i++) {
       EEPROM.write(i,0);
   }
   EEPROM.write(32,60); //D
   EEPROM.write(38,62); //D
   EEPROM.write(44,64); //E
   EEPROM.write(50,65); //F
   EEPROM.write(56,67); //G
   EEPROM.write(62,69); //A
   EEPROM.write(68,71); //H
   EEPROM.write(74,72); //C
   EEPROM.write(31,8); //Laststep

   EEPROM.write(1,0); //maxOfftempoCV
   EEPROM.write(2,0); //maxOffCV1
   EEPROM.write(3,0); //maxOffCV2   
   EEPROM.write(4,1); //outchannel
   EEPROM.write(5,1); //inchannel
   
   EEPROM.write(8,3); //transposemode
   EEPROM.write(9,1); //noteoffmode
   EEPROM.write(10,30); //notelength
   EEPROM.write(11,3); //notethrumode
   EEPROM.write(12,1); //controlthrumode
   EEPROM.write(13,0); //programthrumode
   EEPROM.write(14,1); //bendthrumode
   EEPROM.write(15,0); //otherthrumode

   maxOffTempoCV=EEPROM.read(1);
   maxOffCV1=EEPROM.read(2);
   maxOffCV2=EEPROM.read(3);
   outchannel=EEPROM.read(4);
   inchannel=EEPROM.read(5);

   transposemode=EEPROM.read(8);
   noteoffmode=EEPROM.read(9);
   notelength=EEPROM.read(10);
   notethrumode=EEPROM.read(11);
   controlthrumode=EEPROM.read(12);
   programthrumode=EEPROM.read(13);
   bendthrumode=EEPROM.read(14);
   otherthrumode=EEPROM.read(15);
   
   clearscreen();
   loadsequence();
   
  // */
  
}

void savesequence() {
   lcd.setCursor(1,1);
   lcd.print(F("               "));
   EEPROM.write(31,lastStep);
   for (stepNumber=0; stepNumber < maxSteps; stepNumber++) {
      for (noteInChord=0; noteInChord < maxNotes; noteInChord++) {
          int address=((stepNumber * maxNotes ) + noteInChord) + 32;
          EEPROM.write(address,note[stepNumber][noteInChord]);
          lcd.setCursor(1,1);
          lcd.print(address);
      }
   }
   lcd.setCursor(1,1);
   lcd.print(F("DONE     "));  
}

void loadsequence() {
   lcd.setCursor(0,0);
   lcd.print(F("LOAD SEQUENCE"));
   lastStep=EEPROM.read(31);
   for (stepNumber=0; stepNumber < maxSteps; stepNumber++) {
      for (noteInChord=0; noteInChord < maxNotes; noteInChord++) {
          int address=((stepNumber * maxNotes ) + noteInChord) + 32;
          note[stepNumber][noteInChord]=EEPROM.read(address);
          if ( note[stepNumber][0] == 254 ) lastStep=stepNumber - 1; 
          lcd.setCursor(1,1);
          lcd.print(address);
      }
   }
   lcd.setCursor(1, 1); 
//   lcd.print(F("LOAD DONE      "));
   delay(1000);
   clearscreen();
}


void loop() {
   /* READ BUTTONS */ 
   button1State=!digitalRead(button1Pin);
   button2State=!digitalRead(button2Pin);
   button3State=!digitalRead(button3Pin);
   button4State=!digitalRead(button4Pin);
   button5State=!digitalRead(button5Pin);
   unsigned long CV1raw=analogRead(CV1Pin) ;
   unsigned long CV2raw=analogRead(CV2Pin) ;
   unsigned long TempoCVraw=analogRead(tempoCVInPin) ;
   
   velocity=(CV1 / 8) + 1;
   CV2=(CV2 / 4) + 1;
   /* GENERAT THE INTERNAL CLOCK */
   thistime=millis();
   BPMf=analogRead(tempoCVInPin);
   BPMf=round(BPMf*0.24);
   BPMi=BPMf+1;
   tempoX=round(7500/BPMi);  
   //tempoX=round(15000/BPMi);  
   
   if (clocktime + tempoX <= thistime && clockOn == 0) {
     clockOn=1;
     clocktime=thistime;
     digitalWrite(clockOutPin, HIGH);
     clockOff=0;
   }
   if (clocktime + tempoX <= thistime && clockOff == 0) {
     clockOff=1;
     clocktime=thistime;
     digitalWrite(clockOutPin, LOW);
     clockOn=0;
   }    

   //*/



   /* TIE plus REST will switch to setup*/
   if (button4State == 1 && button4StatePrev == 0 && button5State == 1 && button5StatePrev == 0) {
      button4StatePrev=1;
      button5StatePrev=1;
      clearscreen();
      menu=1;
   }
   if (button4State == 0 && button4StatePrev == 1 && button5State == 0 && button5StatePrev == 1) {
      button4StatePrev=0;
      button5StatePrev=0;
   }
   

   if (menu == 0) { //STOP-PLAY-WRITE
      
      /* TIE BUTTON SENDS ALLNOTESOFF */
      if (button4State == 1 && button4StatePrev == 0 && writemode == 0) {
         button4StatePrev=1;
         midireset();
      }
      if (button4State == 0 && button4StatePrev == 1 && writemode == 0) {
         button4StatePrev=0;
      }
      
      /* PLAY BUTTON */
      if (button1State == 1 && button1StatePrev == 0 && runmode==0 && writemode == 0) { //PLAY
         button1StatePrev=1;
         //clocktime=thistime - 4;
         runmode=1;
         writemode=0; 
         clearscreen();
         if (button2State == 0) stepNumber=0; // DO NOT RESET THE SEQUENCE IF LEAVING THE SETUP 
         lcd.setCursor(0, 0);
         lcd.print(F("PLAY"));           
         //delay(2);
      }
      if (button1State == 0 && button1StatePrev == 1)  {
         button1StatePrev=0;
      }

      /* STOP BUTTON */
      if (button2State == 1 && button2StatePrev == 0 && writemode == 0) { //STOP
         button2StatePrev=1;
         midireset();
         runmode=0;
         clockInStatePrev=0;
         ontrigger=0;
         offtrigger=0;
         pause=0; 
         waitEndOfPause=0;
         stepNumber=0;
         noteInChord=0;
         //clearscreen();
      }
      if (button2State == 0 && button2StatePrev == 1)  {
//        clearscreen();
        button2StatePrev=0;
      }
      
      /* WRITE BUTTON */
      if (button3State == 1 && button3StatePrev == 0 && runmode == 0 && writemode == 0) { //WRITE
         button3StatePrev=1;
         runmode=0;
         writemode=1; 
         stepNumber=0;
         clearscreen(); 
      }
      if (button3State == 0 && button3StatePrev == 1)  button3StatePrev=0;
   }
   
   if (menu > 0 ) { //SETUP
      lcd.setCursor(0,1);
      if (edit == 0) lcd.print(F(" ")); 
      if (edit == 1) lcd.print(F("!")); 

      /* STOP+PLAY exits the setup */
      if ( button1State == 1 && button1StatePrev == 0 && button2State == 1 && button2StatePrev == 0 && menu > 0 && edit == 0) { 
         button1StatePrev = 1;
         button2StatePrev = 1;
         clearscreen();
         lcd.setCursor(0, 0); 
         if (runmode == 0) lcd.print(F("STOP"));
         if (runmode == 1) lcd.print(F("PLAY"));
         menu=0;
      }

      
       /*
       * Stepping thru the setup menu
       */
      
      maxmenu=14; // MAXMENU DETERMINES HOW MANY MENU PAGES ARE AVAILABLE 
         
      if (edit == 0) {
         if (button4State == 1 && button4StatePrev == 0) { // SCROLL THRU MENU DOWN
            button4StatePrev=1; 
            clearscreen();
            menu--;
            if (menu < 1) menu=maxmenu;
         }

         if (button4State == 0 && button4StatePrev == 1) {
            button4StatePrev=0;
         }
         
         if (button5State == 1 && button5StatePrev == 0) { // SCROLL THRU MENU UP
            button5StatePrev=1;
            clearscreen();
            menu++;
            if (menu > maxmenu) menu = 1;
         }
         if (button5State == 0 && button5StatePrev == 1) {
            button5StatePrev=0;
         }
         
         if (button3State == 1 && button3StatePrev == 0) { // ENTER EDIT MODE
            button3StatePrev=1;
            edit=1;           
         }
         if (button3State == 0 && button3StatePrev == 1) {
            button3StatePrev=0;
         }
      }
      switch (menu) {
        /*PLAYMODE,Transpose,NoteOffMode,Notelength,OutChannel,InChannel,NoteThru,ControlThru,ProgrammThru,BendTHru,OtherChannelThru,TrimmPotis,FactoryReset,LoadSequence,SaveSequenc};
         *0        1         2           3          4          5         6        7           8            9        10               11         12           13          ,14
        */
        case 1: //TRANSPOSE
           lcd.setCursor(0, 0); 
           lcd.print(F("TRANSPOSE"));
           lcd.setCursor(1, 1); 
           if (transposemode==0) lcd.print(F("Off          "));
           if (transposemode==1) lcd.print(F("Continous    "));
           if (transposemode==2) lcd.print(F("Momentary    "));
           if (transposemode==3) lcd.print(F("ButtonEnabled "));
           if (edit == 1) {
              if (button4State == 1 && button4StatePrev == 0) { //DECREASE TRANSPOSE MODE
                 button4StatePrev=1;
                 if (transposemode > 0) transposemode--;
              }
              if (button4State == 0 && button4StatePrev == 1) {
                 button4StatePrev=0;
              }
      
              if (button5State == 1 && button5StatePrev == 0) { //INCREASE TRANSPOSE MODE
                 button5StatePrev=1;
                 if (transposemode < 3) transposemode++;
              }
              if (button5State == 0 && button5StatePrev == 1) {
                 button5StatePrev=0;
              }
              
              if (button3State == 1 && button3StatePrev == 0) { 
                 button3StatePrev=1;
                 if (transposemode == 0) transpose=0;
                 EEPROM.write(8,transposemode);
                 delay(10);
                 edit=0;
              }
              if (button3State == 0 && button3StatePrev == 1) {
                 button3StatePrev=0;
              }
           }                      
        break;

        case 2: //NOTEOFFMODE
           lcd.setCursor(0, 0); 
           lcd.print(F("NOTEOFF-MODE"));
           
           lcd.setCursor(1, 1); 
           if (noteoffmode==0) lcd.print(F("Held Note    "));
           if (noteoffmode==1) lcd.print(F("Gated fix    "));
           if (noteoffmode==2) lcd.print(F("Gated CV-ctrl"));
           
           //lcd.setCursor(15, 1); 
           //lcd.print(noteoffmode);
           if (edit == 1) {
              if (button4State == 1 && button4StatePrev == 0) { //DECREASE NOTEOFFMODE
                 button4StatePrev=1;
                 if (noteoffmode > 0) noteoffmode--;
              }
              if (button4State == 0 && button4StatePrev == 1) {
                 button4StatePrev=0;
              }
      
              if (button5State == 1 && button5StatePrev == 0) { //INCREASE NOTEOFFMODE
                 button5StatePrev=1;
                 if (noteoffmode < 2) noteoffmode++;
              }
              if (button5State == 0 && button5StatePrev == 1) {
                 button5StatePrev=0;
              }
              if (button3State == 1 && button3StatePrev == 0) { 
                 button3StatePrev=1;
                 EEPROM.write(9,noteoffmode);
                 delay(10);
                 edit=0;
              }
              if (button3State == 0 && button3StatePrev == 1) {
                 button3StatePrev=0;
              }
           }                      
        break;

        case 3://NOTE LENGTH
           lcd.setCursor(0, 0); 
           lcd.print(F("NOTE LENGTH"));
           lcd.setCursor(1, 1); 
           lcd.print(notelength);
           if (edit == 1) {
              if (button4State == 1 && button4StatePrev == 0) { //DECREASE CHANNEL
                 button4StatePrev=1;
                 if (notelength < 21 && notelength > 1) {
                    notelength = notelength - 1;
                 }
                 if (notelength >= 30) notelength = notelength - 10;
                 //lcd.setCursor(1, 1); 
                 //lcd.print(F("      "));
                 lcd.setCursor(1, 1); 
                 lcd.print(notelength);                 
                 lcd.print(F(" "));                 
              }
              if (button4State == 0 && button4StatePrev == 1) {
                 button4StatePrev=0;
              }
      
              if (button5State == 1 && button5StatePrev == 0) { //INCREASE CHANNEL
                 button5StatePrev=1;
                 if (notelength > 19 && notelength < 240) notelength = notelength + 10;
                 if (notelength < 20) notelength = notelength + 1;
                 lcd.setCursor(1, 1); 
                 lcd.print(F("      "));
                 lcd.setCursor(1, 1); 
                 lcd.print(notelength);                 
                 lcd.print(F(" "));                 
              }
              if (button5State == 0 && button5StatePrev == 1) {
                 button5StatePrev=0;
              }
              if (button3State == 1 && button3StatePrev == 0) { 
                 button3StatePrev=1;
                 EEPROM.write(10,notelength);
                 delay(10);
                 edit=0;
              }
              if (button3State == 0 && button3StatePrev == 1) {
                 button3StatePrev=0;
              }
           }                      
        break;
            
        case 4://OUTCHANNEL
           lcd.setCursor(0, 0); 
           lcd.print(F("OUTPUT CHANNEL"));
           lcd.setCursor(1, 1);
           //String stringChannel=String(outchannel);
           //stringChannel=stringChannel + "           ";
           lcd.print(outchannel);
           if (edit == 1) {
              if (button4State == 1 && button4StatePrev == 0) { //DECREASE CHANNEL
                 button4StatePrev=1;
                 if (outchannel > 1) outchannel--;
                 lcd.setCursor(1, 1); 
                 lcd.print(F("      "));
                 lcd.setCursor(1, 1); 
                 lcd.print(outchannel);                 

              }
              if (button4State == 0 && button4StatePrev == 1) {
                 button4StatePrev=0;
              }
      
              if (button5State == 1 && button5StatePrev == 0) { //INCREASE CHANNEL
                 button5StatePrev=1;
                 if (outchannel < 16) outchannel++;
                 lcd.setCursor(1, 1); 
                 lcd.print(F("      "));
                 lcd.setCursor(1, 1); 
                 lcd.print(outchannel);              }
              if (button5State == 0 && button5StatePrev == 1) {
                 button5StatePrev=0;
              }
              if (button3State == 1 && button3StatePrev == 0) { 
                 button3StatePrev=1;
                 EEPROM.write(4,outchannel);
                 delay(10);
                 edit=0;
              }
              if (button3State == 0 && button3StatePrev == 1) {
                 button3StatePrev=0;
              }
           }                      
        break;

        case 5://INCHANNEL
           lcd.setCursor(0, 0); 
           lcd.print(F("INPUT CHANNEL"));
           lcd.setCursor(1, 1);
           lcd.print(inchannel);
           if (edit == 1) {
              if (button4State == 1 && button4StatePrev == 0) { //DECREASE CHANNEL
                 button4StatePrev=1;
                 if (inchannel > 0) inchannel--;
                 lcd.setCursor(1, 1); 
                 lcd.print(F("      "));
                 lcd.setCursor(1, 1); 
                 lcd.print(inchannel);              }
              if (button4State == 0 && button4StatePrev == 1) {
                 button4StatePrev=0;
              }
      
              if (button5State == 1 && button5StatePrev == 0) { //INCREASE CHANNEL
                 button5StatePrev=1;
                 if (inchannel < 16) inchannel++;
                 lcd.setCursor(1, 1); 
                 lcd.print(F("      "));
                 lcd.setCursor(1, 1); 
                 lcd.print(inchannel);              }
              if (button5State == 0 && button5StatePrev == 1) {
                 button5StatePrev=0;
              }
              if (button3State == 1 && button3StatePrev == 0) { 
                 button3StatePrev=1;
                 EEPROM.write(5,inchannel);
                 delay(10);
                 edit=0;
              }
              if (button3State == 0 && button3StatePrev == 1) {
                 button3StatePrev=0;
              }
           }                      
        break;

        case 6: //NOTETHRU
           lcd.setCursor(0, 0); 
           lcd.print(F("NOTE-THRU-MODE"));
           lcd.setCursor(1, 1);
           if (notethrumode == 0) lcd.print(F("Off          ")); 
           if (notethrumode == 1) lcd.print(F("Record       ")); 
           if (notethrumode == 2) lcd.print(F("Play         ")); 
           if (notethrumode == 3) lcd.print(F("Rec+Play     ")); 
           if (notethrumode == 4) lcd.print(F("PlayTriggered")); 
           if (edit == 1) {
              if (button4State == 1 && button4StatePrev == 0) { //DECREASE MIDI-THRU-MODE
                 button4StatePrev=1;
                 if (notethrumode > 0) notethrumode--;
              }
              if (button4State == 0 && button4StatePrev == 1) {
                 button4StatePrev=0;
              }
              if (button5State == 1 && button5StatePrev == 0) { //INCREASE MIDI-THRU-MODE
                 button5StatePrev=1;
                 if (notethrumode < 4) notethrumode++;
              }
              if (button5State == 0 && button5StatePrev == 1) {
                 button5StatePrev=0;
              }
              
              if (button3State == 1 && button3StatePrev == 0) { 
                 button3StatePrev=1;
                 EEPROM.write(11,notethrumode);
                 delay(10);
                 edit=0;
              }
              if (button3State == 0 && button3StatePrev == 1) {
                 button3StatePrev=0;
              }
           }                      
        break;

        case 7: //CONTROLTHRU
           lcd.setCursor(0, 0); 
           lcd.print(F("CTRL-THRU-MODE"));
           lcd.setCursor(1, 1);
           if (controlthrumode == 0) lcd.print(F("Off")); 
           if (controlthrumode == 1) lcd.print(F("On ")); 
           if (edit == 1) {
              if (button4State == 1 && button4StatePrev == 0) { //DECREASE MIDI-THRU-MODE
                 button4StatePrev=1;
                 if (controlthrumode > 0) controlthrumode--;
              }
              if (button4State == 0 && button4StatePrev == 1) {
                 button4StatePrev=0;
              }
              if (button5State == 1 && button5StatePrev == 0) { //INCREASE MIDI-THRU-MODE
                 button5StatePrev=1;
                 if (controlthrumode < 1) controlthrumode++;
              }
              if (button5State == 0 && button5StatePrev == 1) {
                 button5StatePrev=0;
              }
              
              if (button3State == 1 && button3StatePrev == 0) { 
                 button3StatePrev=1;
                 EEPROM.write(12,controlthrumode);
                 delay(10);
                 edit=0;
              }
              if (button3State == 0 && button3StatePrev == 1) {
                 button3StatePrev=0;
              }
           }                      
        break;

        case 8: //PROGRAM
           lcd.setCursor(0, 0); 
           lcd.print(F("PRG-THRU-MODE"));
           lcd.setCursor(1, 1); 
           //lcd.print(onoffmodename[programthrumode]);          
           if (programthrumode == 0) lcd.print(F("Off")); 
           if (programthrumode == 1) lcd.print(F("On ")); 
           if (edit == 1) {
              if (button4State == 1 && button4StatePrev == 0) { //DECREASE MIDI-THRU-MODE
                 button4StatePrev=1;
                 if (programthrumode > 0) programthrumode--;
              }
              if (button4State == 0 && button4StatePrev == 1) {
                 button4StatePrev=0;
              }
              if (button5State == 1 && button5StatePrev == 0) { //INCREASE MIDI-THRU-MODE
                 button5StatePrev=1;
                 if (programthrumode < 1) programthrumode++;
              }
              if (button5State == 0 && button5StatePrev == 1) {
                 button5StatePrev=0;
              }
              
              if (button3State == 1 && button3StatePrev == 0) { 
                 button3StatePrev=1;
                 EEPROM.write(13,programthrumode);
                 delay(10);
                 edit=0;
              }
              if (button3State == 0 && button3StatePrev == 1) {
                 button3StatePrev=0;
              }
           }                      
        break;

        case 9: //BENDER
           lcd.setCursor(0, 0); 
           lcd.print(F("BEND-THRU-MODE"));
           lcd.setCursor(1, 1); 
           if (bendthrumode == 0) lcd.print(F("Off")); 
           if (bendthrumode == 1) lcd.print(F("On ")); 
           if (edit == 1) {
              if (button4State == 1 && button4StatePrev == 0) { //DECREASE MIDI-THRU-MODE
                 button4StatePrev=1;
                 if (bendthrumode > 0) bendthrumode--;
              }
              if (button4State == 0 && button4StatePrev == 1) {
                 button4StatePrev=0;
              }
              if (button5State == 1 && button5StatePrev == 0) { //INCREASE MIDI-THRU-MODE
                 button5StatePrev=1;
                 if (bendthrumode < 1) bendthrumode++;
              }
              if (button5State == 0 && button5StatePrev == 1) {
                 button5StatePrev=0;
              }
              
              if (button3State == 1 && button3StatePrev == 0) { 
                 button3StatePrev=1;
                 EEPROM.write(14,bendthrumode);
                 delay(10);
                 edit=0;
              }
              if (button3State == 0 && button3StatePrev == 1) {
                 button3StatePrev=0;
              }
           }                      
        break;

        case 10: //OTHER-CHANNELS-THRU
           lcd.setCursor(0, 0); 
           lcd.print(F("OTHER-CH-THRU"));
           lcd.setCursor(1, 1);
           if (otherthrumode == 0) lcd.print(F("Off")); 
           if (otherthrumode == 1) lcd.print(F("On ")); 
           if (edit == 1) {
              if (button4State == 1 && button4StatePrev == 0) { //DECREASE MIDI-THRU-MODE
                 button4StatePrev=1;
                 if (otherthrumode > 0) otherthrumode--;
              }
              if (button4State == 0 && button4StatePrev == 1) {
                 button4StatePrev=0;
              }
              if (button5State == 1 && button5StatePrev == 0) { //INCREASE MIDI-THRU-MODE
                 button5StatePrev=1;
                 if (otherthrumode < 4) otherthrumode++;
              }
              if (button5State == 0 && button5StatePrev == 1) {
                 button5StatePrev=0;
              }
              
              if (button3State == 1 && button3StatePrev == 0) { 
                 button3StatePrev=1;
                 EEPROM.write(15,otherthrumode); 
                 edit=0;
              }
              if (button3State == 0 && button3StatePrev == 1) {
                 button3StatePrev=0;
              }
           }                      
        break;

        case 11: //POTENTIOMETER ADJUSTMENT
           if (edit == 0) {
              lcd.setCursor(0, 0); 
              lcd.print(F("ADJUST POTI-CVs"));
              lcd.setCursor(1, 1); 
              lcd.print(F("PRESS WRITE")); 
           }          
         
              if (button3State == 1 && button3StatePrev == 0) {
                 button3StatePrev=1;
                 adjuststep=0;
                 edit=1;
              }
              if (button3State == 0 && button3StatePrev == 1) {
                 button3StatePrev=0;
              }
              if (edit == 1) {
                 adjustpotis();
              }
        break;
        
        case 12: //FACTORY RESET
           if (edit == 0) {
              lcd.setCursor(0, 0); 
              lcd.print(F("FACTORY RESET"));
              lcd.setCursor(1, 1); 
              lcd.print(F("PRESS 2xWRITE"));          
           }
           if (edit == 1) {
              lcd.setCursor(0, 0); 
              lcd.print(F("RESET:PRESS WRITE"));
              lcd.setCursor(1, 1); 
              lcd.print(F("STOP TO CANCEL"));          
              if (button3State == 1 && button3StatePrev == 0) {
                 runmode = 0;
                 button3StatePrev=1;
                 factoryreset();
                 delay(10);
                 edit=0;
                 clearscreen();
                 menu=0;          
              }
              if (button3State == 0 && button3StatePrev == 1) {
                 button3StatePrev=0;
              }
              if (button2State == 1 && button2StatePrev == 0) { 
                 edit=0;
                 button2StatePrev=1;
              }
              if (button2State == 0 && button2StatePrev == 1) {
                 button2StatePrev=0;
              }
           }                      
        break;


        case 13: //LOAD
           if (edit == 0) {
              lcd.setCursor(0, 0); 
              lcd.print(F("LOAD SEQUENCE"));
              lcd.setCursor(1, 1); 
              lcd.print(F("PRESS 2xWRITE"));
           }
           if (edit == 1) {
              lcd.setCursor(0, 0); 
              lcd.print(F("LOAD PRESS WRITE"));
              lcd.setCursor(1, 1); 
              lcd.print(F("STOP FOR CANCEL"));          
              if (button3State == 1 && button3StatePrev == 0) {
                 runmode = 0;
                 button3StatePrev=1;
                 loadsequence();
                 delay(10);
                 edit=0;
                 clearscreen();
                 menu=0;          
              }
              if (button3State == 0 && button3StatePrev == 1) {
                 button3StatePrev=0;
              }
              if (button2State == 1 && button2StatePrev == 0) { 
                 edit=0;
                 button2StatePrev=1;
              }
              if (button2State == 0 && button2StatePrev == 1) {
                 button2StatePrev=0;
              }

           }                      
        break;

        case 14: //SAVE SEQUENCE
           if (edit == 0) {        
              lcd.setCursor(0, 0); 
              lcd.print(F("SAVE SEQUENCE"));
              lcd.setCursor(1, 1); 
              lcd.print(F("PRESS 2xWRITE"));
           }          
           if (edit == 1) {
              lcd.setCursor(0, 0); 
              lcd.print(F("SAVE PRESS WRITE"));
              lcd.setCursor(1, 1); 
              lcd.print(F("STOP FOR CANCEL"));          
              if (button3State == 1 && button3StatePrev == 0) { 
                 button3StatePrev=1;
                 savesequence();
                 edit=0;
                 lcd.setCursor(1, 1); 
                 lcd.print(F("SAVE DONE    "));
                 if (runmode == 0) delay(500);
                 clearscreen();
                 menu=0;          
              }
              if (button3State == 0 && button3StatePrev == 1) {
                 button3StatePrev=0;
              }
              if (button2State == 1 && button2StatePrev == 0) { 
                 edit=0;
                 button2StatePrev=1;
              }
              if (button2State == 0 && button2StatePrev == 1) {
                 button2StatePrev=0;
              }
           }                      
        break;


        } // END OF SWITCH:CASE
   }  // END OF SETUP       
         
 



   /*NOTE-THRU in STOP */
   if (runmode == 0 && writemode == 0 && notethrumode > 0) { //MIDITHRU in STOP
   /* DISPLAY THE TEMPO IN STOP MODE - DISABLED DUE TO LOW MEMORY CAN BE 
    * MIGHT BE ADDED AFTER MEMORY OPTIMISATION
      if (menu == 0) { //DONT PRINT TEMPO IN SETUP MODE
      lcd.setCursor(5,0);
      lcd.print(BPMi);
      pos=(String(BPMi).length());
      if (menu == 0) { //DONT PRINT TEMPO IN SETUP MODE
         lcd.setCursor(pos+5,0);
         lcd.print(F(" "));  
      }
      }
      //*/
      if (MIDI.read()) {
      byte type=MIDI.getType();
         switch (type) {
            case midi::NoteOn:
               MIDI.sendNoteOn(MIDI.getData1(),MIDI.getData2(),outchannel);
            break;
            case midi::NoteOff:
               MIDI.sendNoteOff(MIDI.getData1(),MIDI.getData2(),outchannel);
            break;
            if ( controlthrumode == 1) {
               case midi::ControlChange: MIDI.sendControlChange(MIDI.getData1(),MIDI.getData2(),outchannel); break;
            }
            if ( programthrumode == 1) {
                case midi::ProgramChange: MIDI.sendProgramChange(MIDI.getData1(),outchannel); break; 
            }
            if ( bendthrumode == 1) {
               case midi::PitchBend: MIDI.sendPitchBend( (((MIDI.getData2() * 128) + MIDI.getData1() ) + 8192),outchannel);
            }

         }
      }
   }

   if (runmode == 0 && writemode == 0 && menu == 0) {
         lcd.setCursor(0, 0);
         lcd.print(F("STOP"));  
   }
   /* WRITE */
   if (runmode == 0 && writemode == 1) { //WRITE
      if ( menu == 0 ) {
         lcd.setCursor(0, 0);
         lcd.print(F("WRITE"));  
      }
      //if (stepNumber < 0) stepNumber=0; //THIS IS ONLY A WORKAROUND FOR THE NOT CORRECT BUTTON HANDLING OF WRITE BUTTON
      stepLCDposition=stepNumber % 16;
      /* WRITE NoteOn and NoteOff */
     if (MIDI.read()) { 
         byte type=MIDI.getType()  ;                  
         if (type == midi::NoteOn ) {
            if (notethrumode == 1 || notethrumode == 3 ) MIDI.sendNoteOn(MIDI.getData1(),velocity,outchannel);
            note[stepNumber][noteInChord]=MIDI.getData1();
            noteInChord++;
            keysPressed++;
            maxNoteInChord=noteInChord;

            lcd.setCursor(13,0);                  
            lcd.print(stepNumber+1);
         }
      

         if (type == midi::NoteOff ) {
               if (notethrumode == 1 || notethrumode == 3 ) MIDI.sendNoteOff(MIDI.getData1(),0,outchannel);
               //noteInChord--;
               keysPressed--;
               if (keysPressed == 0) { 
                  if (maxNoteInChord <= maxNotes - 1) {
                     int i=0;
                     for (i=maxNoteInChord; i <= maxNotes ; i++) {
                        note[stepNumber][i]=128; //Note value=128 is defined a no note - all notesInChord=128 are a rest
                     }
                 }
                 //lcd.setCursor(stepNumber,1);
                 noteInChord=0;
                 printstep(stepLCDposition,43); // Print a dot "+"
                 stepNumber++;
               }
         }
      } //if (MIDI.read())
      
      /* Write a TIE */
      
      if (button4State == 1 && noteInChord == 0 && button4StatePrev == 0) {
         int i=0;
         for (i=0; i <= maxNotes ; i++) {
            note[stepNumber][i]=129;
            noteInChord++;
         }
         button4StatePrev=1;
         delay(2);
         lcd.setCursor(13,0);
         //lcd.print(F("   "));
         lcd.print(stepNumber+1);
         printstep(stepLCDposition, 45); //print a "-"
         stepNumber++;
      }
      if (button4State == 0 && button4StatePrev == 1) {
         button4StatePrev=0;
         noteInChord=0;
      }

      /* Write a REST */
      
      if (button5State == 1 && noteInChord == 0 && button5StatePrev == 0) {
         int i=0;
         for (i=0; i < maxNotes ; i++) {
            note[stepNumber][i]=128;
            noteInChord++;
         }
         button5StatePrev=1;
         delay(2);
         lcd.setCursor(13,0);
         //lcd.print(F("   "));
         lcd.print(stepNumber+1);
         printstep(stepLCDposition,46); // Print a "."
         stepNumber++;
      }
      if (button5State == 0 && button5StatePrev == 1) {
         button5StatePrev=0;
         noteInChord=0;
      }

      /* One Step Backward with WRITE */
     
      if (button3State == 1 && noteInChord == 0 && button3StatePrev == 0 && writemode == 1 && stepNumber > 0) {
         button3StatePrev=1;
         stepBack=1;
      }
      if (button3State == 0 && button3StatePrev == 1) {
         button3StatePrev=0;
      }

      if (stepBack == 1 ) {
         stepNumber--;
         printstep(stepLCDposition - 1, 32); //print a " "
         lcd.setCursor(13,0);
         //lcd.print(F("   "));
         lcd.print(stepNumber);
         noteInChord=0;
         stepBack=0;
      }
      //*/
      if (button1State == 1 || button2State == 1 || stepNumber == maxSteps) { // stop-write-condition
         if (stepNumber > 0 ) { 
            lastStep=stepNumber;
         }
         
         writemode=0;
      } // stop-write-condition
   } //if (writemode == 1)


   
/* PLAY */



   if (runmode == 1 && writemode == 0) { //PLAY
      if (button1State == 1 && button1StatePrev == 0 && pause == 0 && menu == 0 && waitEndOfPause == 0 ) { // ENTER PAUSE MODE
         button1StatePrev=1;
         pause=1;
         lcd.setCursor(0,0);
         lcd.print(F("play"));
      }
      if (button1State == 0 && button1StatePrev == 1) {
         button1StatePrev=0;
      }
      if (button1State == 1 && button1StatePrev == 0 && pause == 1 && menu == 0 && waitEndOfPause == 0) { // ENTER WAIT PAUSE END MODE
         button1StatePrev=1;
         waitEndOfPause=1;
         lcd.setCursor(0,0);
         lcd.print(F("plAY"));
      }
      if (waitEndOfPause == 1 && button1State == 1 && button1StatePrev == 0 ) {
         pause=0;
         waitEndOfPause=0;
         button1StatePrev=1;
         lcd.setCursor(0,0);
         lcd.print(F("PLAY"));
      }
      if (stepNumber == 0 && pause == 1 && waitEndOfPause == 1) {
         pause=0;
         waitEndOfPause=0;
         lcd.setCursor(0,0);
         lcd.print(F("PLAY"));
         }         
    
      /* DISPLAY THE TEMPO IN PLAY MODE - Nice not have, but may cause PERFORMACE ISSUES 
      lcd.setCursor(5,0);
      lcd.print(BPMi);
      pos=(String(BPMi).length());
      lcd.setCursor(pos+5,0);
      lcd.print(F(" "));
      //*/
      /* 
      if ( menu == 0 && pause==0) {
         lcd.setCursor(0, 0);
         lcd.print(F("PLAY"));
      }
      //*/

      /* WRITE BUTTON JUMPS TO NOTEOFF-MODE SETUP*/
      if (button3State == 1 && button3StatePrev == 0 && runmode == 1 && writemode == 0) { 
         button3StatePrev=1;
         menu=2;
         clearscreen(); 
      }
      if (button3State == 0 && button3StatePrev == 1)  button3StatePrev=0;


      
      /* MIDI Thru while PLAY and MIDI In for Transpose*/
    
      if (MIDI.read()) {
      byte type= MIDI.getType();
      switch (type) { 
            case midi::NoteOn:
               if ((notethrumode == 2 || notethrumode == 3) && (transposemode == 0 || (transposemode == 3 && button5State == 0))) MIDI.sendNoteOn(MIDI.getData1(),MIDI.getData2(),outchannel);
               if (transposemode == 1 || transposemode == 2) {
                  transpose=MIDI.getData1();
                  transpose=transpose - 60;
                  showtranspose(transpose);
               }
               if (transposemode == 3 && button5State == 1) {
                  transpose=MIDI.getData1();
                  transpose=transpose - 60;
                  showtranspose(transpose);               

               }
            break;
            case midi::NoteOff:
               if ((notethrumode == 2 || notethrumode == 3) && (transposemode == 0 || (transposemode == 3 && button5State == 0))) MIDI.sendNoteOff(MIDI.getData1(),MIDI.getData2(),outchannel);
               if (transposemode == 2) {
                transpose=0;
                showtranspose(transpose);
               }
            break;
            if ( controlthrumode == 1) {
               case midi::ControlChange: MIDI.sendControlChange(MIDI.getData1(),MIDI.getData2(),outchannel); break;
            }
            if ( programthrumode == 1) {
                case midi::ProgramChange: MIDI.sendProgramChange(MIDI.getData1(),outchannel); break; 
            }
            if ( bendthrumode == 1) {
               case midi::PitchBend: MIDI.sendPitchBend( (((MIDI.getData2() * 128) + MIDI.getData1() ) + 8192),outchannel);
            }

      }
   }
     /*  
      *   Read external trigger
      */
      triggerState=!digitalRead(clockInPin);
      if (triggerState == 1 && clockInStatePrev == 0) {
         ontrigger=1;
         clockInStatePrev=1;
         triggertime=thistime;
      }
      if (triggerState == 0 && clockInStatePrev == 1 && noteoffmode == 1) {
         offtrigger=1;
      }
      if ((noteoffmode == 2 && thistime >= triggertime + CV2) || (noteoffmode == 2 && ontrigger==1 && clockInStatePrev == 0)) {
         offtrigger=1;
      }


      if (triggerState == 0 && clockInStatePrev == 1) clockInStatePrev=0;

      

     /* 
      *  Play Notes
      */
      if (ontrigger == 1 && offtrigger == 0) {
         /*   NoteOff im Held-Mode     */
         if (noteoffmode == 0) {
         for (noteInChord=0; noteInChord < maxNotes; noteInChord++) {
            // If the last step was note and this next step is a Note or a REST then send the separately stored NoteOff
            if (noteOff[noteInChord] < 128 && note[stepNumber][0] < 129) {
               MIDI.sendNoteOff(noteOff[noteInChord],velocity,outchannel);
            }
            // If the last step was a TIE (129) and the next step is not a TIE then send the separately stored NoteOff 
            if (note[stepNumber - 1][noteInChord] == 129 && note[stepNumber][noteInChord] < 129) {
              MIDI.sendNoteOff(noteOff[noteInChord],velocity,outchannel);
            }
         } 
         } // END OF noteoffmode == 0 */
         
         /* send the note on for the actual step */
         for (noteInChord=0; noteInChord < maxNotes; noteInChord++) {
            if (note[stepNumber][noteInChord]<= 127 && note[stepNumber][noteInChord] > 0) {
               if (pause == 0) {
                 MIDI.sendNoteOn(note[stepNumber][noteInChord] + transpose,velocity,outchannel);
                 noteOff[noteInChord]=note[stepNumber][noteInChord] + transpose;
               }
            }
         }
         ontrigger=0;

         /* The following is a nice display, but not required */
         if(1==1) {
         stepLCDposition=stepNumber % 16;
         if (note[stepNumber][0] < 128) {
          printstep(stepLCDposition,43); //print a "+"
         }
         if (note[stepNumber][0] == 128) {
          printstep(stepLCDposition,46); //print a "-"
         }
         if (note[stepNumber][0] == 129) {
          printstep(stepLCDposition,45); //print a "." 
         } 
         /* */  
         lcd.setCursor(13,0);
         lcd.print(F("   "));
         lcd.setCursor(13,0);
         lcd.print(stepNumber+1);
         //*/
         }
         
         if (stepNumber <= lastStep) stepNumber++;

      } //END of (ontrigger == 1 && offtrigger == 0)    
      /* Note Off in Gated Mode */  
      if (offtrigger == 1 && ontrigger== 0 && noteoffmode > 0) { 
         /* This delay sets a minimum note length */
         //if (noteoffmode == 1) delay(notelength);
         //if (noteoffmode == 2) delay(CV2);  
         for (noteInChord=0; noteInChord <= maxNotes - 1 ; noteInChord++) {
            /* if this step is not the last step and this step is a note and the next step is not a TIE then send noteoff*/
            if (stepNumber - 1 < lastStep && noteOff[noteInChord] <= 127 && note[stepNumber ][noteInChord] <= 128)
            {             
              MIDI.sendNoteOff(noteOff[noteInChord],velocity,outchannel);
            }
            /* if this is the last step and this step is a note and the first step is not a TIE then send noteoff*/
            if (stepNumber - 1 == (lastStep - 1) && noteOff[noteInChord] <= 127 && note[0][noteInChord] <= 128)
            {             
              MIDI.sendNoteOff(noteOff[noteInChord],velocity,outchannel);
            }
            
            /* if this is not the last step and this step is a TIE (129) and the next is not a TIE  then send NoteOff for the note from step before */
            if (stepNumber - 1 < lastStep && note[stepNumber - 1][noteInChord] == 129 && note[stepNumber][noteInChord] <= 128 ) 
            {
              MIDI.sendNoteOff(noteOff[noteInChord],0,outchannel);
            }

            /* if this step is the last step and this step is a TIE (129) and the fist step is not a TIE  then send NoteOff for the note from step before */
            if (stepNumber - 1 == (lastStep - 1) && note[stepNumber - 1][noteInChord] == 129 && note[0][noteInChord] <= 128 ) 
            {
              MIDI.sendNoteOff(noteOff[noteInChord],0,outchannel);
            }
         } // END of noteOff-send for-loop
         offtrigger=0;

      } // END of (offtrigger == 1 && ontrigger== 0 && noteoffmode > 0)
    if (stepNumber == lastStep) stepNumber=0;
  } //if (runmode == 1)
} // EndOfLoop
