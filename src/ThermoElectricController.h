/*

Software to control a tec on the controller board.

Should:
accept power commands
Manage direction control.  Set current to zero before switching direction.
Report Thermistor ADC values to serial or over Ethernet port  (this would be a good place for a web server!)

*/

#ifndef __ThermoElectricController_H
#define __ThermoElectricController_H

#include "Arduino.h"
#include "ThermoElectricGlobal.h"

const int TEC_PWM_FREQ = 50000;

class ThermoElectricController {
 public:  
  ThermoElectricController();
  int begin ( const int dirPin, const int pwmPin, const int thermistorPin);
  
  int setPower( const int percent );
  //void setDirection( const bool direction );
  float getTemperature();
  int getPower();
  bool getDirection();
 protected:
  void setPwm(int power);
  float temperature; // cooked ADC value
  int thermistor; // raw ADC value
  int pwmPct;
  bool dir;
  int dirPin;
  int pwmPin;
  int thermistorPin;
  int thermistorResistor;
  int raw_data;

};

class Thermistor: public ThermoElectricController {
  public:
    bool calibrate(float ref_temp, int tempNum, const int thermistor);
    bool load_cal_data();
    bool clear_calibration();
    float getRaw_low();
    float getRaw_high();
    void setRaw_low(float low);
    void setRaw_high(float high);
    int eeAddr;
    bool calibrated = false;

  private:
    float raw_Low;
    float raw_High;
};


bool hardwareID_init();
int get_hardware_id();



#endif