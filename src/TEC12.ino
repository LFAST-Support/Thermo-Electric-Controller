/*

Software to control the 12-tec controller board.

Should:

Accept power commands through Ethernet or serial line (simulated)
Manage direction control.  Set current to zero before switching direction.
Report Thermistor ADC values to serial or over Ethernet port  (this would be a good place for a web server!)
*/

#include "Arduino.h"
#include "ThermoElectricController.h"
#include "ThermoElectricGlobal.h"

/******************
 * Begin Configure
 ******************/

struct tec_pins {
  int dirPin;
  int pwmPin;
  int thermistorPin;
};

struct tec_pins pins[] =
  {
   // dir, pwm, thermistor
   {12, 0,23},
   {26,3,20}
  };

   /*
   {12, 0,23},
   {24,1,22},
   {25,2,21},
   {26,3,20},
   {27,4,19},
   {28,5,18},
   {29,6,17},
   {30,7,16},
   {31,8,15},
   {32,9,14},
   {37,10,41},
   {36,11,40}  
  };

  23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 41, 40
 ******************
 * End Configure
 ******************/
ThermoElectricController TEC[NUM_TEC];
Thermistor therm[NUM_TEC];

ThermoElectricController* TEC_ptr = TEC;


void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  delay(1000);
  Serial.println("Configuring the TECs");
  // setup the TECs
  delay(10000);
  for (int i = 0; i < NUM_TEC; i++ ) {
    TEC[i].begin( pins[i].dirPin, pins[i].pwmPin, pins[i].thermistorPin );
  }

  //Load cal data if thermistors have been calibrated.
  if (EEPROM.read(0) == 0x01) {
    therm->load_cal_data();
  }
  Serial.print("Configured "); Serial.print(NUM_TEC); Serial.println(" TEC current controllers");
  delay(1000);
}




int blink = 0; 

void loop() {

  for(int j = -100 ; j <= 100; j+=20 ) {	
    for (int i = 0; i < NUM_TEC; i++) {
      TEC[i].setPower(j);
      Serial.print("TEC["); Serial.print( i ); Serial.print("] temp = "); Serial.print(TEC[i].getTemperature());
      Serial.print(" Power = ");Serial.print(TEC[i].getPower());
      Serial.print(" Dir = ");Serial.println(TEC[i].getDirection());
    }
    Serial.println();
    delay(1000);
    digitalWrite(LED_BUILTIN, (blink++ & 0x01)); 
  }
}
