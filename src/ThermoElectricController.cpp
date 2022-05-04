/* 
  Implementation of the TEC control software
*/
#include "ThermoElectricController.h"
#include "ThermoElectricGlobal.h"

ThermoElectricController::ThermoElectricController() {
}

int ThermoElectricController::begin( const int dirP, const int pwmP, const int thermistorP )
{
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
  thermistor = 0.0 ;
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


void ThermoElectricController::setPwm( int power )
{
  int tmp = (int) ((float) power * 255.0)/100.0;// convert to 0-255 )
    analogWrite( pwmPin,tmp); //
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
//

float  ThermoElectricController::getTemperature( void ) {
  // read the analog voltage
  int adcCounts = 0;
  //Serial.print("Getting Temperature from pin ");Serial.println( thermistorPin );
  for(int i = 0; i < 16; i++)
    adcCounts += analogRead(thermistorPin);
  adcCounts = adcCounts >> 4;  // divide by 16
  
  // convert to voltage 
  float voltage = adcCounts * 3.3/4096.0;
  // convert to temperature
  temperature = voltage * 1.0;  // FIXME 
  return temperature;
}

int  ThermoElectricController::getPower( void ) {
  return pwmPct;
}

bool  ThermoElectricController::getDirection( void ) {
  return dir;
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
