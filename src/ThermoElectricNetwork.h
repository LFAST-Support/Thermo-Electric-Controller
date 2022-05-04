/*******************************************************************************
Copyright 2021
Steward Observatory Engineering & Technical Services, University of Arizona

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <https://www.gnu.org/licenses/>.
*******************************************************************************/

/**
 * @file thermistorMux_network.h
 * @author Nestor Garcia (Nestor212@email.arizona.edu)
 * @brief Networking specific definitions and function prototypes. For NTP,
 * MQTT, Sparkplug, and Ethernet. 
 * Originally created for VCM module, modified for thermistor Mux use
 * @version (see THERMISTOR_MUX_VERSION in thermistorMux_global.h)
 * @date 2022-04-19
 *
 * @copyright Copyright (c) 2021
 */

#ifndef THERMISTORMUX_NETWORK_H
#define THERMISTORMUX_NETWORK_H

// Public functions
bool network_init();
void check_brokers();
void publish_data(float* thermistor_data, float ADC_temperature);
bool update_ntp();
unsigned long get_current_time();
unsigned long long get_current_time_millis();
void decode_cal_data();
void publish_calibration_status(bool);


#endif
