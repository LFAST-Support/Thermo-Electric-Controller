/*

Software to control the 12-tec controller board.

Should:

Accept power commands through Ethernet or serial line (simulated)
Manage direction control.  Set current to zero before switching direction.
Report Thermistor ADC values to serial or over Ethernet port  (this would be a good place for a web server!)
*/

#include "ThermoElectricController.h"
#include "ThermoElectricGlobal.h"
#include "ThermoElectricNetwork.h"

/******************
 * Begin Configure
 ******************/

struct tec_config {
  int dirPin;
  int pwmPin;
  int thermistorPin;
  bool thermistor ; // is there a thermistor or Seebeck temperature
  int minimum_percent ; // the Diodes, inc parts only go from about 15 percent to 100 percent
};

struct tec_config tec_cfg[]=
{
  // dir, pwm, thermistor or seebeck, min
  {12,0,23,0,15},
  {24,1,22,0,15},
  {25,2,21,0,15},
  {26,3,20,0,0},
  {27,4,19,0,0},
  {28,5,18,0,0},
  {29,6,17,0,0},
  {30,7,16,0,0},
  {31,8,15,0,0},
  {32,9,14,0,0},
  {37,10,41,0,0},
  {36,11,40,0,0},
};
/*
 ******************
 * End Configure
 ******************/

ThermoElectricController TEC[NUM_TEC];
Thermistor therm[NUM_TEC];
bool calibrated = false;
int blink = 0; 

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  delay(1000);
  Serial.println("Configuring the TECs");

  //Load cal data if thermistors have been calibrated.
  if (EEPROM.read(0) == 0x01) {
    Serial.println("Retrieving Cal Data.");
    therm->load_cal_data();
    calibrated = true;
  }

  //setup the TECs
  delay(10000);
  for (int i = 0; i < NUM_TEC; i++ ) {
    TEC[i].begin( tec_cfg[i].dirPin, tec_cfg[i].pwmPin, tec_cfg[i].thermistorPin, 
                  tec_cfg[i].thermistor, tec_cfg[i].minimum_percent );
  }
  Serial.print("Configured "); Serial.print(NUM_TEC); Serial.println(" TEC current controllers");
  delay(1000);

  bool setup_successful = hardwareID_init() && network_init();
  //Setup MQTT protocol, connect to broker
  if(setup_successful){
    delay(15000);
    check_brokers();
    Serial.println("Setup successful.");
  }
  else {
    Serial.println("Setup Failed.");
  }
}

void loop() {
  //for(int j = -100 ; j <= 100; j+=50 ) {	
  for (int i = 0; i < NUM_TEC; i++) {
    //TEC[i + 1].setPower(j);
    // Serial.print("TEC["); Serial.print( i ); Serial.print("] temp = "); 
    //Serial.print(" Power = ");Serial.print(TEC[i].getPower());
    //Serial.print(" Dir = ");Serial.println(TEC[i].getDirection());
    /*
    if (calibrated) {
      publish_data(i, TEC[i].getPower(), TEC[i].getDirection(), TEC[i].get_Calibrated_Temp(i));
      Serial.print(".Ino temp: ");Serial.println(TEC[i].get_Calibrated_Temp(i));
    }
    else {
      */
      publish_data(i, TEC[i].getPower(), TEC[i].getDirection(), TEC[i].get_Temperature(i), TEC[i].getSeebeck());
    //}
  }
  digitalWrite(LED_BUILTIN, (blink++ & 0x01)); 
  Serial.println("Publishing Metrics.");
  publish_node_data();
  delay(6000);
  //}
  check_brokers();
}
