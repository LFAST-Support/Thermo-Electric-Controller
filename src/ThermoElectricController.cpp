/* 
  Implementation of the TEC control software
*/
#include "ThermoElectricController.h"
#include "ThermoElectricGlobal.h"

/*
Resistance at 25 degrees C
The beta coefficient of the thermistor (usually 3000-4000)
*/
#ifdef thermistor_10K
    #define THERMISTORNOMINAL 10000
    #define BCOEFFICIENT 2.514458134e-4 // = 1/3977, B = 3997 K
#elif thermistor_2K 
    #define THERMISTORNOMINAL 2200   
    #define BCOEFFICIENT 2.544529262e-4 // = 1/3930, B = 3930 K
#else 
    #error A thermistor value must be defined.
#endif
// temp. for nominal resistance (almost always 25 C = 298.15 K)
#define TEMPERATURENOMINAL 298.15   


ThermoElectricController::ThermoElectricController() {}

int ThermoElectricController::begin( const int dirP, const int pwmP, const int thermistorP ) {
  /*! @brief     Initializes the contents of the class
    @details   Sets pin definitions, and initializes the variables of the class.
    @param[in] dirPin Defines which pin controls direction
    @param[in] pwmPin Defines which pin provides PWM pulses to the TEC
    @param[in] thermistorP Defines which pin provides PWM pulses to the TEC
    @return    void 
  */
  
  dirPin = dirP;
  pwmPin = pwmP;
  thermistorPin = thermistorP;
  temperature = -40.0;
  raw_data = 0.0 ;
  pwmPct = 0;
  dir = 0;
  thermistorResistor = 10000;
  // set the pins properly for this TEC
  pinMode(dirPin,OUTPUT);
  pinMode(pwmPin,OUTPUT);
  pinMode(thermistorPin,INPUT);
  // set up the PWM parameters for the PWM pin.
  analogWriteFrequency(pwmPin, TEC_PWM_FREQ);
  analogReadResolution(12);
  
  return 0;
}

void ThermoElectricController::setPwm( int power ) {
  int tmp = abs ((int) ((float) power * 255.0)/100.0);// convert to 0-255, negative values yield a negative result; < 0
    analogWrite( pwmPin,tmp ); //
}

int ThermoElectricController::setPower( const int power ) {
  if( power > 100 || power < -100 )
    return -1;
  Serial.print("Setting Power to ");Serial.println( power ); 
  // set direction
  if(((power < 0) && (pwmPct >= 0)) ||
     ((power > 0) && (pwmPct <= 0)))
    { // change needed
      setPwm(0);
      digitalWrite(dirPin, (power < 0));
      dir = (power < 0);
    }
  // Convert to duty cycle
  // Set duty cycle
  setPwm(power);
  pwmPct = power; // save the power setting
  return 0; 
}

// 0 to 3.3 volts, 12 bits resolution
// Need to read and average a bunch of these together to beat down the noise...
float ThermoElectricController::get_Raw_Temperature() {
  // read the analog voltage
  int adcCounts = 0;
  //Serial.print("Getting Temperature from pin ");Serial.println( thermistorPin );
  for(int i = 0; i < 16; i++) {
    adcCounts += analogRead(thermistorPin);
  }
  raw_data = adcCounts >> 4;  // divide by 16
  
  // convert to voltage 
  float voltage = raw_data * 3.3/4096.0;
  //Serial.printf("Voltage: %f\n", voltage);
  // convert to temperature
  
  /*
  thermistance depends on order of resistors in voltage divider circuitry.
  10K Ohm resistor assumed
  */
  float thermistance = 10000 * ((3.3/voltage) - 1); //If voltage drop accross thermistor occurs first.
  //Serial.printf("Thermistance: %f\n", thermistance);

  /*
  Simplified B parameter Steinhart-Hart equation.
  B coefficient for thermistor:  TT7-10KC3-11
  */
  temperature = (1/((1/TEMPERATURENOMINAL) + BCOEFFICIENT*log(thermistance/THERMISTORNOMINAL))) - 273.15;
  //Serial.printf("Temperature: %f\n", temperature);
  return temperature;
}

float ThermoElectricController::get_Calibrated_Temp(int i) {
  extern Thermistor therm[NUM_TEC];
  extern ThermoElectricController TEC[NUMBER_OF_CHANNELS];

  temperature = (((get_Raw_Temperature() - therm[i].getRaw_low()) * (ref_High - ref_Low)) / 
                 (therm[i].getRaw_high() - therm[i].getRaw_low()) + ref_Low);
  Serial.println(therm[i].getRaw_low());
  Serial.println(therm[i].getRaw_high());


  //Serial.printf("Temperature: %f\n", temperature);
  return temperature;
}

int  ThermoElectricController::getPower( void ) {
  return pwmPct;
}

bool  ThermoElectricController::getDirection( void ) {
  return dir;
}


bool Thermistor::calibrate( float ref_temp, int tempNum ) {
  Serial.printf("Set temp is %0.2f, calibration begun.\n", ref_temp); 
  extern ThermoElectricController TEC[NUM_TEC];
  extern Thermistor therm[NUM_TEC];  
  

  if (tempNum == 1) {
    eeAddr = 1;
    Serial.println("Cal data 1 INW");
    ref_Low = ref_temp;
    EEPROM.put(eeAddr, ref_Low);
    eeAddr += sizeof(ref_Low); 
    for (int i = 0; i < NUM_TEC; i++) { 
      therm[i].raw_Low = TEC[i].get_Raw_Temperature();
      EEPROM.put(eeAddr, therm[i].raw_Low);
      eeAddr += sizeof(therm[i].raw_Low);
    }
    return false;
  } 
  else if (tempNum == 2) {
    Serial.println("Cal data 2 INW");
    ref_High = ref_temp;
    EEPROM.put(eeAddr, ref_High);
    eeAddr += sizeof(ref_High); 
    for (int i = 0; i < NUM_TEC; i++) { 
      therm[i].raw_High = TEC[i].get_Raw_Temperature();
      EEPROM.put(eeAddr, therm[i].raw_High);
      eeAddr += sizeof(therm[i].raw_High); 
      therm[i].calibrated = true;
    }
    EEPROM.write(0, 0x01);
    Serial.println("Calibration complete.");
  } 
  return true;  
}
bool Thermistor::clear_calibration() {
  extern Thermistor therm[NUM_TEC];  
  EEPROM.write(0, 0x00);
  
  for (int i = 0; i < NUM_TEC; i++) { 
    therm[i].raw_Low = 0;
    therm[i].raw_High = 0;
  }


}

float Thermistor::getRaw_low() {
  return raw_Low;
}

float Thermistor::getRaw_high() {
  return raw_High;
}

void Thermistor::setRaw_low(float low) {
  raw_Low = low;
}

void Thermistor::setRaw_high(float high) {
  raw_High = high;
}

bool Thermistor::load_cal_data() {
  extern Thermistor therm[NUM_TEC];
  float temp_data;
  eeAddr = 1;

  EEPROM.get(eeAddr, ref_Low);
  eeAddr += sizeof(ref_Low); 
  for (int i = 0; i < NUM_TEC; i++ ) {
    EEPROM.get(eeAddr, temp_data);
    therm[i].setRaw_low(temp_data);
    eeAddr += sizeof(temp_data); 
  }
  EEPROM.get(eeAddr, ref_High);
  eeAddr += sizeof(ref_High); 
  for (int i = 0; i < NUM_TEC; i++ ) {
    EEPROM.get(eeAddr, temp_data);
    therm[i].setRaw_high(temp_data);
    eeAddr += sizeof(temp_data); 
  }
  return true;
}







bool hardwareID_init(){

  pinMode(ID_PIN_0,INPUT_PULLUP);
  pinMode(ID_PIN_1,INPUT_PULLUP);
  pinMode(ID_PIN_2,INPUT_PULLUP);
  pinMode(ID_PIN_3,INPUT_PULLUP);
  pinMode(ID_PIN_4,INPUT_PULLUP);

  // Wait for pin inputs to settle
  delay(100);

  // Read the jumpers.  This must only be done once, even if they produce an
  // invalid ID, in order to ensure that they are correctly read.
  // The pins have inverted sense, so the raw values have been reversed.
  int pin0 = digitalRead(ID_PIN_0) ? 0 : 1;
  int pin1 = digitalRead(ID_PIN_1) ? 0 : 1;
  int pin2 = digitalRead(ID_PIN_2) ? 0 : 1;
  int pin3 = digitalRead(ID_PIN_3) ? 0 : 1;
  int pin4 = digitalRead(ID_PIN_4) ? 0 : 1;

  hardware_id = ( pin4 << 4 ) +
                ( pin3 << 3 ) +
                ( pin2 << 2 ) +
                ( pin1 << 1 ) +
                ( pin0 << 0 );

  delay(10000);
  DebugPrintNoEOL("Hardware ID = ");
  DebugPrint(hardware_id);

  // Make sure ID is valid
  if(hardware_id < 0 || hardware_id > MAX_BOARD_ID){
      DebugPrint("invalid board ID detected, check jumpers");
      return false;
  }

  return true; //Success
}

int get_hardware_id() {
  return hardware_id;
}
