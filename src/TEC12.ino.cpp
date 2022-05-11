# 1 "C:\\Users\\ngarcia\\AppData\\Local\\Temp\\tmpx1jfy_3o"
#include <Arduino.h>
# 1 "C:/Users/ngarcia/OneDrive - University of Arizona/Desktop/SOML/Projects/ThermoElectric-Controller/src/TEC12.ino"
# 12 "C:/Users/ngarcia/OneDrive - University of Arizona/Desktop/SOML/Projects/ThermoElectric-Controller/src/TEC12.ino"
#include "Arduino.h"
#include "ThermoElectricController.h"
#include "ThermoElectricGlobal.h"





struct tec_pins {
  int dirPin;
  int pwmPin;
  int thermistorPin;
};

struct tec_pins pins[] =
  {

   {12, 0,23},
   {26,3,20}
  };
# 52 "C:/Users/ngarcia/OneDrive - University of Arizona/Desktop/SOML/Projects/ThermoElectric-Controller/src/TEC12.ino"
ThermoElectricController TEC[NUM_TEC];
Thermistor therm[NUM_TEC];

ThermoElectricController* TEC_ptr = TEC;
void setup();
void loop();
#line 58 "C:/Users/ngarcia/OneDrive - University of Arizona/Desktop/SOML/Projects/ThermoElectric-Controller/src/TEC12.ino"
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  delay(1000);
  Serial.println("Configuring the TECs");

  delay(10000);
  for (int i = 0; i < NUM_TEC; i++ ) {
    TEC[i].begin( pins[i].dirPin, pins[i].pwmPin, pins[i].thermistorPin );
  }


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