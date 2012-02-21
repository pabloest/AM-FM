/*
 * Si4735 Advanced Networked Attached Radio Sketch
 * Written by Jon Carrier
 * Based on Si4735_Example sketch code from Ryan Owens for Sparkfun Electronics
 *
 * HARDWARE SETUP:
 * This sketch assumes you are using the Si4735 Shield from SparkFun Electronics.  
 * The shields should be plugged into an Arduino Main Board (Uno, Duemillinove or similar).
 * The radio shield requires logic level conversion in order to communicate with 5V main boards
 *
 * ARDUINO PIN USAGE AND PURPOSE:
 * 0 -  Serial RX (used for remote control through USB)
 * 1 -  Serial TX (used to write to the LCD display)
 * 2 -  ROTARY Encoder A (Also initially acts as the INT_PIN, GPO2)
 * 3 -  ROTARY Encoder B
 * 4 -  
 * 5 -  ROTARY Push Button (Used to switch the local control mode for the rotary encoder)
 * 6 - 
 * 7 -  
 * 8 -  RADIO Power
 * 9 -  RADIO Reset
 * 10 - RADIO Slave Select
 * 11 - SPI MOSI
 * 12 - SPI MISO
 * 13 - SPI CLK
 *
 *
 * NOTES:
 * This sketch uses the Si4735 in FM mode. Other modes are AM, SW and LW. Check out the datasheet for more information on these
 * modes. All of the functions in the library will work regardless of which mode is being used; however the user must indicate
 * which mode is to be used in the begin() function. See the library documentation for more information.
 */

//===================DEFINE LIBRARIES==================
#include <SPI.h>
#include <Si4735.h>
#include <SoftwareSerial.h>
#include <Bounce.h>

//===================Create the Object Instances==================
Si4735 radio;
Station tuned;

//===================DEFINE RADIO Related Parameters=================
#define EncA 3 //Encoder A
#define EncB 2 //Encoder B
#define PB 5 //Pushbutton

// Instantiate a Bounce object with a 5 millisecond debounce time
Bounce bouncer = Bounce(PB, 10); 

bool update=true;

//Define the state of the rotary encoder
int state=0;

//Define the user configurable settings
volatile byte volume=63; //Start at 100% Volume
volatile int frequency=8850; //Start at 100.3MHz
//volatile int frequency=980; //Start at 980 kHz
int oldfrequency = frequency;

unsigned long oldfrequencyT, dwellT, backlightT, backlightdwellT, currentT;

volatile boolean halfleft = false;      // Used in both interrupt routines
volatile boolean halfright = false;

//RBDS INFO
bool ps_rdy;
char ps_prev[9]; //previous ps
char pty_prev[17]="                ";
byte mode=FM; //mode 0 is FM, mode 1 is AM
//byte mode=AM; //mode 0 is FM, mode 1 is AM

unsigned long lastUpdate; //Scrolling Refresh Parameter
byte radioText_pos; //Scrolling Position
bool backlight;

//=========================END OF PARAMETERS==============================

//#####################################################################
//                              SETUP
//#####################################################################
void setup()
{
	//Create a serial connection
	Serial.begin(9600);
        pinMode(2, INPUT);
        digitalWrite(2, HIGH);                // Turn on internal pullup resistor
        pinMode(3, INPUT);
        digitalWrite(3, HIGH);                // Turn on internal pullup resistor
        attachInterrupt(0, isr_2, FALLING);   // Call isr_2 when digital pin 2 goes LOW
        attachInterrupt(1, isr_3, FALLING);   // Call isr_3 when digital pin 3 goes LOW
        
        pinMode(PB, INPUT);
        
        dwellT = 600;
        backlightdwellT = 10000; // LCD backlight dims after 10 seconds of inactivity
        oldfrequencyT = 0;

	//Setup the LCD display and print the initial settings
	backlightOn();
        backlight = true;
	delay(500);
	goTo(0);
	Serial.print("-=ArduinoRadio=-");
	showFREQ();

        //Configure the radio
	radio.begin(mode);
        radio.setLocale(NA); //Use the North American PTY Lookup Table
	radio.tuneFrequency(frequency);
	volume=radio.setVolume(volume);

	lastUpdate = millis();
        backlightT = lastUpdate;
	radioText_pos = 0;
}

//#####################################################################
//                            LOOPING
//#####################################################################
void loop()
{       	     
        //Update and store the RDS information
	ps_rdy=radio.readRDS(); 
	radio.getRDS(&tuned); 
        
        bouncer.update();
        int value = bouncer.read();
        if (value == HIGH) {
          if (!backlight) {
            backlightOn();
            delay(20);
            backlight = true;
            backlightT = millis();
          }
          else if (backlight) {
            switchBand();
          }
        }
        
        if(frequency != oldfrequency) {
          unsigned long frequencyT = millis();
          if (!backlight) {
            backlightOn();
            delay(20);
            backlight = true;
            backlightT = millis();
          }
          else if (backlight) {
            backlightT = millis();
          }
          
          showFREQ();
          if ( (frequencyT - oldfrequencyT) > dwellT) {
            oldfrequency = frequency;
            oldfrequencyT = frequencyT;
            radio.tuneFrequency(frequency);
          }
        }
        
        if (backlight) {
          goTo(16);
          currentT = millis();
          if ( (currentT - backlightT) > backlightdwellT) {
            goTo(16);
            backlightT = currentT;
            backlightOff();
            backlight = false;
          }
        }
//        showPTY();
//        showRadioText();
        showCALLSIGN();
}


//#####################################################################
//                      LCD PRINTING FUNCTIONS
//#####################################################################
//----------------------------------------------------------------------
void showPS(){ //Displays the Program Service Information
        if (strlen(tuned.programService) == 8){
            if(ps_rdy){      
                if(!strcmp(tuned.programService,ps_prev)){         
      		        goTo(16);     
      		        Serial.print("-=[ ");
      		        Serial.print(tuned.programService); 
      		        Serial.print(" ]=-");     
                        strcpy(ps_prev,tuned.programService);
      		}  
            }    
      	}    
      	else {
                 //if(!strcmp("01234567",ps_prev,8)){ 
                        goTo(16); //Serial.print("-=ArduinoRadio=-");
                        Serial.print("                ");
                 //       strcpy(ps_prev,"01234567");      
                 //}
        }
}
//----------------------------------------------------------------------
void showRadioText(){ //Displays the Radio Text Information
        if ((millis() - lastUpdate) > 500) {   
		if (strlen(tuned.radioText) == 64) {
			//The refresh trigger cause the scrolling display to be delayed
			//this allows for the user to observe the new value they changed      
			goTo(16);
			if (radioText_pos < 64 - 16) {
				for (byte i=0; i<16; i++) { Serial.print(tuned.radioText[radioText_pos + i]); }
			} 
			else {
				byte nChars = 64 - radioText_pos;
				for (byte i=0; i<nChars; i++) { Serial.print(tuned.radioText[radioText_pos + i]); }
				for(byte i=0; i<(16 - nChars); i++) { Serial.print(tuned.radioText[i]); }
			}      
			radioText_pos++;
			if(radioText_pos >= 64) radioText_pos = 0;      
		}   
		lastUpdate = millis(); 
	}  
}
void showPTY(){
  if(!strcmp(tuned.programType,pty_prev)){
    goTo(16);
    Serial.print(tuned.programType);
    strcpy(pty_prev,tuned.programType);
  }  
}
//----------------------------------------------------------------------
void showCALLSIGN(){
  goTo(16);
  char Stationcall[5];
  for (int i = 0; i < 5; i++) {
    Stationcall[i] = tuned.callSign[i];
  }
  String Stationcallstring;
  Stationcallstring = String(Stationcall);

  if (Stationcallstring != "UNKN") {
      Serial.print("Station: ");
      Serial.print(Stationcall);
      Serial.print("   ");
  }
  else Serial.print("                ");
}

//----------------------------------------------------------------------
void showFREQ(){ //Displays the Freq information
        selectLineOne();
        if(mode==FM){
	    Serial.print("FM: ");
	    Serial.print((frequency/100));
	    Serial.print(".");
	    Serial.print((frequency%100)/10);
            if (frequency < 9999) {
              Serial.print(" MHz    "); 
            } else Serial.print(" MHz   ");
        }
        else{
            Serial.print("AM: ");
	    Serial.print((frequency));	    
	    Serial.print(" kHz");
            if (frequency < 1000) {
              Serial.print("     ");
            } else Serial.print("    ");
        }
        delay(50);
}

//#####################################################################
//                      REMOTE CONTROL FUNCTIONS
//#####################################################################
void switchBand() {
  radio.end();
  delay(50);
  clearLine2();
  clearLine1();
  Serial.print("Switching to ");
  if(mode==AM){ 
          mode=FM;
          Serial.print("FM");
          frequency=8850;
  }
  else{ 
          mode=AM;
          Serial.print("AM"); 
          frequency=680;
  }
  delay(250);  
  radio.begin(mode);	
  delay(100);
  radio.tuneFrequency(frequency);
}

int debounce(int signal, int debounceTime){
  int state = digitalRead(signal);
  int lastState = !(state);
  while (state != lastState) {
    lastState=state;
    delay(debounceTime);
    state = digitalRead(signal);
  } 
  return state;
}

//=====================================================================
//                           LCD FUNCTIONS
//=====================================================================

//SerLCD Helper Functions
void selectLineOne(){  //puts the cursor at line 0 char 0.
   Serial.write(0xFE);   //command flag
   Serial.write(128);    //position  
}

void selectLineTwo(){  //puts the cursor at line 0 char 0.
   Serial.write(0xFE);   //command flag
   Serial.write(192);    //position   
}

void goTo(int position) { //position = line 1: 0-15, line 2: 16-31, 31+ defaults back to 0
    if (position<16){ 
      Serial.write(0xFE);   //command flag
      Serial.write((position+128));    //position
    }
    else if (position<32){
      Serial.write(0xFE);   //command flag
      Serial.write((position+48+128));    //position 
    }
    else { 
      goTo(0); 
    }   
}

void clearLCD(){
   Serial.write(0xFE);   //command flag
   Serial.write(0x01);   //clear command.   
}

void backlightOn(){  //turns on the backlight
    Serial.write(0x7C);   //command flag for backlight stuff
    Serial.write(157);    //light level.   
    delay(10);
}

void backlightOff(){  //turns off the backlight
    Serial.write(0x7C);   //command flag for backlight stuff
    Serial.write(128);     //light level for off.  
    delay(10); 
}

void backlightFadeOn() {
  int steps;
  steps = 157-128; // from min to max brightness
  for (int i=0; i < steps; i++)
  {
    Serial.write(0x7C);
    Serial.write(128+i);
    delay(5);
  }
  backlight = true;
}

void backlightFadeOff() {
  int steps;
  steps = 157-128; // from max to min brightness
  for (int i=0; i < steps; i++)
  {
    Serial.write(0x7C);
    Serial.write(157-i);
    delay(50);
  }
//  backlightOff();
  backlight = false;
}

void serCommand(){   //a general function to call the command flag for issuing all other commands   
  Serial.write(0xFE);
}

void clearLine1(){
  goTo(0);
  Serial.write("                ");
  goTo(0);
}

void clearLine2(){
  goTo(16);
  Serial.write("                ");
  goTo(16);
}

void isr_2(){                                              // Pin2 went LOW
  delay(1);                                                // Debounce time
  if(digitalRead(2) == LOW){                               // Pin2 still LOW ?
    if(digitalRead(3) == HIGH && halfright == false){      // -->
      halfright = true;                                    // One half click clockwise
    }  
    if(digitalRead(3) == LOW && halfleft == true){         // <--
      halfleft = false;      // One whole click counter-
      if (mode==FM) {
        frequency-=20;                                            // clockwise
      }
      else frequency-=10;
    }
  }
}
void isr_3(){                                             // Pin3 went LOW
  delay(1);                                               // Debounce time
  if(digitalRead(3) == LOW){                              // Pin3 still LOW ?
    if(digitalRead(2) == HIGH && halfleft == false){      // <--
      halfleft = true;                                    // One half  click counter-
    }                                                     // clockwise
    if(digitalRead(2) == LOW && halfright == true){       // -->
      halfright = false;      // One whole click clockwise
      if (mode==FM) {
        frequency+=20;
      }
      else frequency+=10;
    }
  }
}

