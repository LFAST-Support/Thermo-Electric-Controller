# Thermo-Electric Controller
This project contains Thermo-Electric Controller(TEC) card firmware for the LFAST project at the Steward Observatory, University of Arizona.
The thermo-electric controller runs on a Teensy 4.1, which commands 12 TEC channels on the circuit card assembly. Every channel has the capability to house a thermistor
and monitor temperature, or measure the seebeck voltage accross two point. Additionally, each channel can output a power value between -100 and 100.

Calibration of thermistors is not required, but a calibration routine exists for more precise temperature data. Calibration data is then stored into Teensy EEPROM address: 1..., until cleared by user through client. EEPROM address 0 serves as calibration status flag. 
    If EEPROM.read(0) == 0x01, the TEC has been calibrated. 

## Dependencies
* Arduino.h 
* Ethernet.h 
* EEPROM.h
* MATH.h
* PubSubClient (SO-ETS fork, in https://github.com/Steward-Observatory-ETS/pubsubclient)
* NTPClient_Generic.h
* sparkplugb_arduino.hpp
    
Install Arduino IDE + Teensyduino. Teensyduino can be found at the following page: https://www.pjrc.com/teensy/td_download.html
 
NTPClient_Generic can be downloaded from the Arduino IDE's library manager.
Select Tools -> Manage Libraries and search for `NTPClient_Generic`.  I am
using NTPClient_Generic 3.2.2.

This code uses the SO-ETS fork of the PubSubClient library, which adds support
for binary Will messages, required for Sparkplug.  This fork can be downloaded
from https://github.com/Steward-Observatory-ETS/pubsubclient.  All of the files
and directories should then be copied to your Arduino IDE's sketchbook library
folder.  You can find or change the location of your sketchbook folder in the
Arduino IDE menu: File > Preferences > Sketchbook location.  The PubSubClient
library files should then be stored in the libraries/PubSubClient folder under
that sketchbook location.  Note: Do not download or update PubSubClient from
the Arduino IDE's library manager, as this will overwrite the SO-ETS fork.

sparkplugb_arduino is a package containing the C source files for the Eclipse Tahu project with a simple
helper class.  This is maintained by Steward Observatory Engineering and Technical Services (ETS) and is located in the repository:
https://github.com/Steward-Observatory-ETS/sparkplugb_arduino


The entire sparkplugb_arduino folder needs to be placed in the arduino folder,
either the user or system folder should work.  Mine is placed in
`Documents/Arduino/libraries` on my Windows 10 computer.

## Test Client
* The client requires a connection to an MQTT broker. Eclipse Mosquitto was utilized during the writing and testing of the TEC client and firmware. 
* For instructions on installing a mosquitto broker, follow the link below. 
*       https://mosquitto.org/download/
*       
* The `test_environment` folder contains a test client program `client.py` written in Python.  This is an MQTT client that can be used to send MQTT commands via an MQTT broker to a TEC module and/or display MQTT messages published by the TEC module.  It currently only runs as a command-line interface.
* The Test Client has data logging capabilities.  Inbound messages from the TEC data topic are optionally logged to a CSV file with filename `TEC_test_log_YYYY-MM-DD.csv` in the folder where the Test Client is run.
*
* The client can be run as a command-line interface or GUI option. Both offer the same features and display the same telemetry. 
* Telemetry:
*   Power of each TEC channel
*   Direction of each TEC channel current
*   Either thermistor temperature or seebeck voltage

* For detailed instructions on installing the necessary libraries, setting up the test environment and running this application, refer to `test_environment/test_client_Notes.txt`.


**Built-in Calibration Tests**
* Calibration of the thermistors must be accomplished through the client. 
* Calibration is split into two parts, to gather two temperature extremes for a linear calibration function calculation.   
* Entering the following commands into the command-line client will accomplish the calibration:
*   calibrate temp1: Thermistors are placed at 0 celsius (or low extreme) and raw_Low temp is collected & stored into EEPROM by firmware, ref_Low is stored in EEPROM.
*   calibrate temp2: Thermistors are placed at 100 celsius (or high extreme) and raw_High temp is collected & stored into EEPROM by firmware, ref_High is stored in EEPROM.
*           Calibrated_Temp = [((raw_Temp - raw_Low) * (ref_Range) / (raw_Range)] + ref_Low;
* Source: https://learn.adafruit.com/calibrating-sensors/two-point-calibration

* The calibration routine is also accessible through the client GUI, which functions in a similar fashion as the command-line interface. 


## Testing 
* Unit tests for this firmware are currently in work.


**Viewing Sparkplug Data with MQTT.fx**
* MQTT.fx is a powerful tool which can be used to subscribe to MQTT topics and parse Sparkplug B payloads.
* https://softblade.de/en/mqtt-fx/


