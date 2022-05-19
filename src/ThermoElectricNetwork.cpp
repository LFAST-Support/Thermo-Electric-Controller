/*******************************************************************************
Copyright 2021
Steward Observatory Engineering & Channelhnical Services, University of Arizona

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
 * @file ChannelTEC_network.cpp
 * @author Nestor Garcia (Nestor212@email.arizona.edu)
 * @brief Implements networking specific functions, to handle Ethernet, MQTT,
 * Sparkplug, and NTP functionality.
 * Originally created for VCM module, modified for Channel TEC use.
 * @version (see Channel_TEC_VERSION in ChannelTEC_global.h)
 * @date 2022-04-19
 *
 * @copyright Copyright (c) 2021
 */

#include "ThermoElectricNetwork.h"
#include "ThermoElectricGlobal.h"
#include "ThermoElectricController.h"
#include "cf_sparkplug.h"
#include <NativeEthernet.h>
#include <PubSubClient.h>
#include <NTPClient_Generic.h>
#include <sparkplugb_arduino.hpp>

// Reset defines
#ifndef RESTART_ADDR
#define RESTART_ADDR 0xE000ED0C
#endif
#define READ_RESTART() (*(volatile uint32_t *)RESTART_ADDR)
#define WRITE_RESTART(val) ((*(volatile uint32_t *)RESTART_ADDR) = (val))

#define TEC_VERSION_COMPLETE "1v1"

// Common network configuration values: TBD
#define GATEWAY 128, 96, 11, 233
#define SUBNET 255, 255, 0, 0
#define DNS 128, 96, 11, 233
#define NUM_BROKERS  1

#if defined(production_TEST)
// MQTT broker definitions: TBD
//Nestors office mosquitto broker
//#define MQTT_BROKER1 169,254,141,48

//#define MQTT_BROKER1 'localhost'

//Nestors laptop mosquitto broker
#define MQTT_BROKER1 169,254,31,208
#define MQTT_BROKER1_PORT 1883

//NTP server address
#define NTP_IP  {169, 254, 39, 226}

#else
  #error A network configuration must be defined
#endif

// MAC address for device #0, adjusted according to ID pins: TBD
#define TEC0_MAC  {0xa, 0x0, 0x0, 0x0, 0x0, 0x0}

// IP address for device #0, adjusted according to ID pins: TBD
#define TEC0_IP   {169, 254, 84, 177}

// Sparkplug settings
#define GROUP_ID              "VI"              // This node's group ID
#define NODE_I_tempLATE      "TECx"            // Template for this node's node ID
#define NODE_ID_TOKEN         'x'               // Character to be replaced with module ID

/*
  Private variables
*/
// NTP variables
static EthernetUDP ntpUDP;
static IPAddress ntpIP = NTP_IP;
// Can use "pool.ntp.org" if connected to internet, but might have outages.
// TODO: decrease sync frequency to avoid violation of pool.ntp.org terms of service
static NTPClient ntp(ntpUDP, ntpIP);

// MQTT variables
static EthernetClient enet[NUM_BROKERS];
static PubSubClient m_broker[NUM_BROKERS];

// Sparkplug node and topic names
static String node_id        = NODE_I_tempLATE;
static String nodeBirthTopic = NODE_TOPIC(NBIRTH_MESSAGE_TYPE, NODE_I_tempLATE);
static String nodeDeathTopic = NODE_TOPIC(NDEATH_MESSAGE_TYPE, NODE_I_tempLATE);
static String nodeDataTopic  = NODE_TOPIC(NDATA_MESSAGE_TYPE,  NODE_I_tempLATE);
static String nodeCmdTopic   = NODE_TOPIC(NCMD_MESSAGE_TYPE,   NODE_I_tempLATE);

// These variables hold the last published value of each metric
static uint64_t m_bdSeq[NUM_BROKERS]  = {0};  // Node birth/death sequence numbers
static bool     m_nodeReboot          = false;
static bool     m_nodeRebirth         = false;
static bool     m_nodeNextServer      = false;
static bool     m_nodeClearCal        = false;
static bool     m_nodeCalibrated      = false;
static bool     m_nodeCalibrationINW  = false;
static uint64_t m_commsVersion        = COMMS_VERSION;
static const char *m_firmwareVersion  = TEC_VERSION_COMPLETE;
static float    m_calTemp1            = {0.0};
static float    m_calTemp2            = {0.0};
static const char *m_units            = "/" ;// The user units
static float m_Channel_pwr[NUMBER_OF_CHANNELS] = {0.00};
static bool m_Channel_dir[NUMBER_OF_CHANNELS] = {false};
static float m_Channel_temp[NUMBER_OF_CHANNELS] = {0.00};


// Alias numbers for each of the node metrics
enum NodeMetricAlias {
    NMA_bdSeq = 0,
    NMA_Reboot,
    NMA_Rebirth,
    NMA_NextServer,
    NMA_ClearCal,
    NMA_CalibrationStatus,
    NMA_CalibrationTemp1,
    NMA_CalibrationTemp2,
    NMA_CalibrationINW,
    NMA_CommsVersion,
    NMA_FirmwareVersion,
    NMA_Units,
    NMA_Channel0_pwr,
    NMA_Channel1_pwr,
    NMA_Channel2_pwr,
    NMA_Channel3_pwr,
    NMA_Channel4_pwr,
    NMA_Channel5_pwr,
    NMA_Channel6_pwr,
    NMA_Channel7_pwr,
    NMA_Channel8_pwr,
    NMA_Channel9_pwr,
    NMA_Channel10_pwr,
    NMA_Channel11_pwr,
    NMA_Channel0_dir,
    NMA_Channel1_dir,
    NMA_Channel2_dir,
    NMA_Channel3_dir,
    NMA_Channel4_dir,
    NMA_Channel5_dir,
    NMA_Channel6_dir,
    NMA_Channel7_dir,
    NMA_Channel8_dir,
    NMA_Channel9_dir,
    NMA_Channel10_dir,
    NMA_Channel11_dir,
    NMA_Channel0_temp,
    NMA_Channel1_temp,
    NMA_Channel2_temp,
    NMA_Channel3_temp,
    NMA_Channel4_temp,
    NMA_Channel5_temp,
    NMA_Channel6_temp,
    NMA_Channel7_temp,
    NMA_Channel8_temp,
    NMA_Channel9_temp,
    NMA_Channel10_temp,
    NMA_Channel11_temp,
    EndNodeMetricAlias
};

// The bdseq metric for a single broker
static MetricSpec bdseqMetricsTemplate[] = {
    {"bdSeq", NMA_bdSeq, false, METRIC_DATA_TYPE_INT64, NULL, false, 0},
};

// The bdseq metrics for all brokers
static MetricSpec bdseqMetrics[NUM_BROKERS][NUM_ELEM(bdseqMetricsTemplate)];

// All node metrics
static MetricSpec NodeMetrics[] = {
    {"Node Control/Reboot",                       NMA_Reboot,                 true, METRIC_DATA_TYPE_BOOLEAN,  &m_nodeReboot,             false, 0},
    {"Node Control/Rebirth",                      NMA_Rebirth,                true, METRIC_DATA_TYPE_BOOLEAN,  &m_nodeRebirth,            false, 0},
    {"Node Control/Next Server",                  NMA_NextServer,             true, METRIC_DATA_TYPE_BOOLEAN,  &m_nodeNextServer,         false, 0},
    {"Node Control/Calibration INW",              NMA_CalibrationINW,         true, METRIC_DATA_TYPE_BOOLEAN,  &m_nodeCalibrationINW,     false, 0},
    {"Node Control/Clear Cal Data",               NMA_ClearCal,               true, METRIC_DATA_TYPE_BOOLEAN,  &m_nodeClearCal,           false, 0},
    {"Properties/Calibration Status",             NMA_CalibrationStatus,      true, METRIC_DATA_TYPE_BOOLEAN,  &m_nodeCalibrated,         false, 0},
    {"Node Control/Calibration Temperature 1",    NMA_CalibrationTemp1,       true, METRIC_DATA_TYPE_FLOAT,    &m_calTemp1,               false, 0},        
    {"Node Control/Calibration Temperature 2",    NMA_CalibrationTemp2,       true, METRIC_DATA_TYPE_FLOAT,    &m_calTemp2,               false, 0},    
    {"Properties/Communications Version",         NMA_CommsVersion,           false, METRIC_DATA_TYPE_INT64,   &m_commsVersion,           false, 0},
    {"Properties/Firmware Version",               NMA_FirmwareVersion,        false, METRIC_DATA_TYPE_STRING,  &m_firmwareVersion,        false, 0},
    {"Properties/Units",                          NMA_Units,                  false, METRIC_DATA_TYPE_STRING,  &m_units,                  false, 0},
    {"Outputs/Power Channel0",                    NMA_Channel0_pwr,           true, METRIC_DATA_TYPE_FLOAT,    &m_Channel_pwr[0],         false, 0},
    {"Outputs/Power Channel1",                    NMA_Channel1_pwr,           true, METRIC_DATA_TYPE_FLOAT,    &m_Channel_pwr[1],         false, 0},
    {"Outputs/Power Channel2",                    NMA_Channel2_pwr,           true, METRIC_DATA_TYPE_FLOAT,    &m_Channel_pwr[2],         false, 0},
    {"Outputs/Power Channel3",                    NMA_Channel3_pwr,           true, METRIC_DATA_TYPE_FLOAT,    &m_Channel_pwr[3],         false, 0},
    {"Outputs/Power Channel4",                    NMA_Channel4_pwr,           true, METRIC_DATA_TYPE_FLOAT,    &m_Channel_pwr[4],         false, 0},
    {"Outputs/Power Channel5",                    NMA_Channel5_pwr,           true, METRIC_DATA_TYPE_FLOAT,    &m_Channel_pwr[5],         false, 0},
    {"Outputs/Power Channel6",                    NMA_Channel6_pwr,           true, METRIC_DATA_TYPE_FLOAT,    &m_Channel_pwr[6],         false, 0},
    {"Outputs/Power Channel7",                    NMA_Channel7_pwr,           true, METRIC_DATA_TYPE_FLOAT,    &m_Channel_pwr[7],         false, 0},
    {"Outputs/Power Channel8",                    NMA_Channel8_pwr,           true, METRIC_DATA_TYPE_FLOAT,    &m_Channel_pwr[8],         false, 0},
    {"Outputs/Power Channel9",                    NMA_Channel9_pwr,           true, METRIC_DATA_TYPE_FLOAT,    &m_Channel_pwr[9],         false, 0},
    {"Outputs/Power Channel10",                   NMA_Channel10_pwr,          true, METRIC_DATA_TYPE_FLOAT,    &m_Channel_pwr[10],        false, 0},
    {"Outputs/Power Channel11",                   NMA_Channel11_pwr,          true, METRIC_DATA_TYPE_FLOAT,    &m_Channel_pwr[11],        false, 0},
    {"Outputs/Direction Channel0",                NMA_Channel0_dir,           false, METRIC_DATA_TYPE_BOOLEAN,   &m_Channel_dir[0],         false, 0},
    {"Outputs/Direction Channel1",                NMA_Channel1_dir,           false, METRIC_DATA_TYPE_BOOLEAN,   &m_Channel_dir[1],         false, 0},
    {"Outputs/Direction Channel2",                NMA_Channel2_dir,           false, METRIC_DATA_TYPE_BOOLEAN,   &m_Channel_dir[2],         false, 0},
    {"Outputs/Direction Channel3",                NMA_Channel3_dir,           false, METRIC_DATA_TYPE_BOOLEAN,   &m_Channel_dir[3],         false, 0},
    {"Outputs/Direction Channel4",                NMA_Channel4_dir,           false, METRIC_DATA_TYPE_BOOLEAN,   &m_Channel_dir[4],         false, 0},
    {"Outputs/Direction Channel5",                NMA_Channel5_dir,           false, METRIC_DATA_TYPE_BOOLEAN,   &m_Channel_dir[5],         false, 0},
    {"Outputs/Direction Channel6",                NMA_Channel6_dir,           false, METRIC_DATA_TYPE_BOOLEAN,   &m_Channel_dir[6],         false, 0},
    {"Outputs/Direction Channel7",                NMA_Channel7_dir,           false, METRIC_DATA_TYPE_BOOLEAN,   &m_Channel_dir[7],         false, 0},
    {"Outputs/Direction Channel8",                NMA_Channel8_dir,           false, METRIC_DATA_TYPE_BOOLEAN,   &m_Channel_dir[8],         false, 0},
    {"Outputs/Direction Channel9",                NMA_Channel9_dir,           false, METRIC_DATA_TYPE_BOOLEAN,   &m_Channel_dir[9],         false, 0},
    {"Outputs/Direction Channel10",               NMA_Channel10_dir,          false, METRIC_DATA_TYPE_BOOLEAN,   &m_Channel_dir[10],        false, 0},
    {"Outputs/Direction Channel11",               NMA_Channel11_dir,          false, METRIC_DATA_TYPE_BOOLEAN,   &m_Channel_dir[11],        false, 0},
    {"Inputs/Temperature Channel0",               NMA_Channel0_temp,          false, METRIC_DATA_TYPE_FLOAT,   &m_Channel_temp[0],        false, 0},
    {"Inputs/Temperature Channel1",               NMA_Channel1_temp,          false, METRIC_DATA_TYPE_FLOAT,   &m_Channel_temp[1],        false, 0},
    {"Inputs/Temperature Channel2",               NMA_Channel2_temp,          false, METRIC_DATA_TYPE_FLOAT,   &m_Channel_temp[2],        false, 0},
    {"Inputs/Temperature Channel3",               NMA_Channel3_temp,          false, METRIC_DATA_TYPE_FLOAT,   &m_Channel_temp[3],        false, 0},
    {"Inputs/Temperature Channel4",               NMA_Channel4_temp,          false, METRIC_DATA_TYPE_FLOAT,   &m_Channel_temp[4],        false, 0},
    {"Inputs/Temperature Channel5",               NMA_Channel5_temp,          false, METRIC_DATA_TYPE_FLOAT,   &m_Channel_temp[5],        false, 0},
    {"Inputs/Temperature Channel6",               NMA_Channel6_temp,          false, METRIC_DATA_TYPE_FLOAT,   &m_Channel_temp[6],        false, 0},
    {"Inputs/Temperature Channel7",               NMA_Channel7_temp,          false, METRIC_DATA_TYPE_FLOAT,   &m_Channel_temp[7],        false, 0},
    {"Inputs/Temperature Channel8",               NMA_Channel8_temp,          false, METRIC_DATA_TYPE_FLOAT,   &m_Channel_temp[8],        false, 0},
    {"Inputs/Temperature Channel9",               NMA_Channel9_temp,          false, METRIC_DATA_TYPE_FLOAT,   &m_Channel_temp[9],        false, 0},
    {"Inputs/Temperature Channel10",              NMA_Channel10_temp,         false, METRIC_DATA_TYPE_FLOAT,   &m_Channel_temp[10],       false, 0},
    {"Inputs/Temperature Channel11",              NMA_Channel11_temp,         false, METRIC_DATA_TYPE_FLOAT,   &m_Channel_temp[11],       false, 0},
};
//Verify validity of this function
void reset_teensy(){
    WRITE_RESTART(0x5FA0004);
}

// The Test Bench device is active if and only if TEST_BENCH_SUPPORT is defined.
// If the Test Bench device is not active then its DBIRTH, DDEATH and DDATA
// messages are never published and incoming DCMD messages are ignored.

// Publish the NBIRTH message and the DBIRTH message for any devices, with all
// metrics specified.
void publish_births(){
    if (EEPROM.read(0) == 0x01) {
        m_nodeCalibrated = true;
    }
    for(int br_idx = 0; br_idx < NUM_BROKERS; br_idx++){
        // Create and publish the NBIRTH message containing the bdseq metric
        // for this broker together with all the node metrics
        set_up_nbirth_payload();
        if(!add_metrics(true, ARRAY_AND_SIZE(bdseqMetrics[br_idx])) ||
           !publish_metrics(&m_broker[br_idx], 1, nodeBirthTopic.c_str(),
                            true, ARRAY_AND_SIZE(NodeMetrics))){
            DebugPrintNoEOL("Failed to publish NBIRTH: ");
            DebugPrint(cf_sparkplug_error);
            // Continue anyway
        }
    }
}

// Publish the NDATA message with any node metrics that have been updated.
void publish_node_data(){
    // Publish any updated metrics in the NDATA message
    set_up_next_payload();
    if(!publish_metrics(ARRAY_AND_SIZE(m_broker), nodeDataTopic.c_str(), false,
                        ARRAY_AND_SIZE(NodeMetrics))){
        // An empty message means we aren't connected to any brokers, while the
        // no metrics message means no metrics have changed since the last time
        // we published - ignore both of these cases
        if(strcmp(cf_sparkplug_error, ""          ) != 0 &&
           strcmp(cf_sparkplug_error, "No metrics") != 0){
            DebugPrintNoEOL("Failed to publish NDATA: ");
            DebugPrint(cf_sparkplug_error);
        }
        return;
    }
}

/**
 * @brief Subscribe to the required topics on the given broker.
 *
 * @param broker the broker we're subscribing with
 * @return true if we successfully subscribe to all topics
 * @return false if we fail to subscribe to one or more topics
 */
bool subscribeTopics(PubSubClient* broker){
    bool success = true;
    if(!broker->subscribe(HOST_STATE_TOPIC)) {
        Serial.println("Failed to subscribe to host state topic.");
        success = false;
    }
    if(!broker->subscribe(nodeCmdTopic.c_str())) {
        Serial.println("Failed to subscribe to node commands.");
        success = false;
    }
    return success;
}

// Connect to the specified broker and send out initial messages.
bool connect_to_broker(PubSubClient *broker, int br_idx){
    // Increment the birth/death sequence number before creating the NDEATH
    // message
    m_bdSeq[br_idx]++;
    if(!update_metric(ARRAY_AND_SIZE(bdseqMetrics[br_idx]), &m_bdSeq[br_idx])) {
        DebugPrint(cf_sparkplug_error);
    }
    // Create the NDEATH message with its metrics
    set_up_ndeath_payload();
    if(!add_metrics(true, ARRAY_AND_SIZE(bdseqMetrics[br_idx]))){
        DebugPrint(cf_sparkplug_error);
        DebugPrint("Failed to add metrics to NDEATH");
        m_bdSeq[br_idx]--;
        return false;
    }

    // Connect to the broker, with the NDEATH message as our "will"
    if(!connect(broker, node_id.c_str(), nodeDeathTopic.c_str())){
        DebugPrint(cf_sparkplug_error);
        m_bdSeq[br_idx]--;
        return false;
    }

    // Subscribe to the topics we're interested in
    if(!subscribeTopics(broker)){
        DebugPrint("Unable to subscribe to topics on broker");
        // Disconnect gracefully from the broker
        disconnect(broker, nodeDeathTopic.c_str());
        return false;
    }

    // Success
    return true;
}

/***
 * @brief Returns the seconds since Jan 1, 1970 from the NTP object.
 *
 * @return unsigned long
***/

unsigned long get_current_time(void){
    return ntp.getUTCEpochTime();
}

/**
 * @brief Returns the milliseconds since Jan 1, 1970 from the NTP object.
 *
 * @return unsigned long long
***/

unsigned long long get_current_time_millis(void){
    return ntp.getUTCEpochMillis();
}


// Check to see if a received message is a Node command (NCMD) message.  If it
// is, handle it and return true, even if it's invalid; otherwise return false.
bool process_node_cmd_message(char* topic, byte* payload, unsigned int len){
    Serial.println("Processing Command.");
    if(strcmp(topic, nodeCmdTopic.c_str()) != 0) {
        // This is not a Node command message
        return false;
    }
    // Decode the Sparkplug payload
    sparkplugb_arduino_decoder decoder;
    if(!decoder.decode(payload, len)){
        // Invalid payload - don't do anything
        decoder.free_payload();
        DebugPrint("Unable to decode Node command payload");
        // This was a Node command message
        return true;
    }


    // Process the metrics
    for(unsigned int idx = 0; idx < decoder.payload.metrics_count; idx++) {
        Metric *metric = &decoder.payload.metrics[idx];
        MetricSpec *metric_spec = find_received_metric(ARRAY_AND_SIZE(NodeMetrics), metric);
        if(metric_spec == NULL){
            // Invalid metric - skip it
            DebugPrintNoEOL("Unrecognized Node metric: ");
            DebugPrint(cf_sparkplug_error);
            continue;
        }

    
        // Now handle the metric
        int64_t alias = metric_spec->alias;
        extern ThermoElectricController TEC[NUM_TEC];
        extern Thermistor therm[NUM_TEC];

        switch(alias){
        case NMA_Reboot:
            if(metric->value.boolean_value) {
                DebugPrint("Reboot command received");
                // Reboot immediately - don't attempt to process the rest of
                // the message, publish data, send death certificate,
                // disconnect from broker, or close network
                reset_teensy();
            }
            break;
        case NMA_Rebirth:
            DebugPrint("Rebirth Command received");
            m_nodeRebirth = metric->value.boolean_value;
            if(!update_metric(ARRAY_AND_SIZE(NodeMetrics), &m_nodeRebirth)) {
                DebugPrint(cf_sparkplug_error);
            }
            if(m_nodeRebirth) {
                publish_births();
                DebugPrint("Node Rebirth command received");
            }
            break;

        case NMA_NextServer:
            //### The Next Server command is part of the Sparkplug spec, but it
            //### has no real use here since we stay connected to all brokers.
            //### Should we just ignore this command (and remove metric and flag)?
            m_nodeNextServer = metric->value.boolean_value;
            if(!update_metric(ARRAY_AND_SIZE(NodeMetrics), &m_nodeNextServer)) {
                DebugPrint(cf_sparkplug_error);
            }
            if(m_nodeNextServer) {
                DebugPrint("NextServer command received");
            }
            break;
        case NMA_CalibrationTemp1:
            m_nodeCalibrationINW = metric->value.boolean_value;
            m_calTemp1 = metric->value.float_value;
            m_nodeCalibrated = false;
            m_nodeCalibrationINW = true;
            therm->calibrate(m_calTemp1, 1);
            for(int br_idx = 0; br_idx < NUM_BROKERS; br_idx++){
                set_up_next_payload();
                publish_metrics(&m_broker[br_idx], 1, nodeBirthTopic.c_str(), true, ARRAY_AND_SIZE(NodeMetrics));
                m_nodeCalibrationINW = false; 
            }
            break;
        case NMA_CalibrationTemp2:
            m_nodeCalibrationINW = metric->value.boolean_value;
            m_calTemp2 = metric->value.float_value;
            m_nodeCalibrated = true;
            therm->calibrate(m_calTemp2, 2);
            for(int br_idx = 0; br_idx < NUM_BROKERS; br_idx++){
                set_up_next_payload();
                publish_metrics(&m_broker[br_idx], 1, nodeBirthTopic.c_str(), true, ARRAY_AND_SIZE(NodeMetrics));
            }
            break;
        case NMA_CalibrationINW:
            m_nodeCalibrationINW = metric->value.boolean_value;
            if(!update_metric(ARRAY_AND_SIZE(NodeMetrics), &m_nodeCalibrationINW)){
                DebugPrint(cf_sparkplug_error);
            }
            break;
        case NMA_ClearCal:
            m_nodeCalibrated = false;
            for(int br_idx = 0; br_idx < NUM_BROKERS; br_idx++){
                set_up_next_payload();
                publish_metrics(&m_broker[br_idx], 1, nodeBirthTopic.c_str(), true, ARRAY_AND_SIZE(NodeMetrics));
            }
            DebugPrint("Calibration data has been permanently erased.");            
            break;
        case NMA_Channel0_pwr ... NMA_Channel11_pwr: {            
            int channel;
            channel = alias - NMA_Channel0_pwr;
            if(channel >= 0 && channel < NUMBER_OF_CHANNELS){
                m_Channel_pwr[channel] = metric->value.float_value;
                //### Should value be limited to min/max here?
                //### It will be limited by set_channel(), but should we report the
                //### commanded (invalid) voltage or the actual voltage set?
                TEC[channel].setPower(m_Channel_pwr[channel]);

                // Publish this TEC value, even if it hasn't changed.  The
                // timestamp should show when the value was last set, not when
                // it last changed.
                //if(!update_metric(ARRAY_AND_SIZE(NodeMetrics), &m_Channel_pwr[channel])) {
                //    DebugPrint(cf_sparkplug_error);
                //}
            }
            Serial.printf("Channel %d set to value %0.2f ", channel, m_Channel_pwr[channel]);            
            break;
        }
        default:
            DebugPrintNoEOL("Unhandled Node metric alias: ");
            DebugPrint(alias);
            break;
        }
    }

    // Free the decoder memory
    decoder.free_payload();

    // This was a Node command message
    return true;
}

/**
 * @brief Callback registered with broker to handle incoming data from
 * subscribed topics.
 *
 * @param topic the subscribed topic that we're getting data from
 * @param payload the incoming payload we're receiving on that topic
 * @param len the length, in bytes, of the payload
 */
void callback_worker(char* topic, byte* payload, unsigned int len){
    // Check parameters are valid
    if(topic == NULL || strcmp(topic, "") == 0){
        // Topic was not specified - don't do anything
        DebugPrint("No topic specified");
        return;
    }
    if(payload == NULL){
        // Payload was not specified - don't do anything
        DebugPrint("No payload specified");
        return;
    }
    if(len == 0){
        // Payload is empty - don't do anything
        DebugPrint("Payload length is zero");
        return;
    }

    // Determine message type
    bool host_online = false;
    if(process_host_state_message(topic, payload, len, &host_online)){
        // A non-empty error indicates the message was invalid
        if(strcmp(cf_sparkplug_error, "") != 0) {
            DebugPrint(cf_sparkplug_error);
        }
        if(host_online){
            // Primary Host is connected to this broker
            DebugPrint("Primary Host is ONLINE");
            //### Should we publish births to let the Primary Host know we're
            //### here, or wait for the Primary Host to send a Rebirth message?
            //### After all, we might have received this message because *we*
            //### just connected to the broker, not the Primary Host.
        }
        else{
            // Primary Host is not connected to this broker
            DebugPrint("Primary Host is OFFLINE");
            //### Enter safe state (not applicable for this module)
        }
    }
    else if(!process_node_cmd_message(topic, payload, len)) {
        // Unrecognized message
        char topic_short[40];
        snprintf(topic_short, sizeof(topic_short), "%s", topic);
        DebugPrintNoEOL("Unrecognized message topic: \"");
        DebugPrintNoEOL(topic_short);
        DebugPrint("\"");
    }
}

/**
 * @brief Publish metrics for TEC channels and temperature.  Note that we
 * publish this data even if it hasn't changed because the timestamp should
 * show when the data was last read, not when it last changed.
 *
 * @param Channel_data an array of NUM_Channel_CHANNELS floats representing the averaged
 * Channel voltages
 * @param the average temperature reading
 */


void publish_data( int channel_num, float channel_pwr, bool channel_dir, float channel_temp ){
    
    // Store new Channel data, converting from raw Channel values to user units
    m_Channel_pwr[channel_num] = channel_pwr;
    m_Channel_dir[channel_num] = channel_dir;
    m_Channel_temp[channel_num] = channel_temp;

    for (int i = 0; i < NUM_TEC; i++) {
        if(!(update_metric(ARRAY_AND_SIZE(NodeMetrics), &m_Channel_pwr[i]) &&
                update_metric(ARRAY_AND_SIZE(NodeMetrics), &m_Channel_dir[i]) &&
                update_metric(ARRAY_AND_SIZE(NodeMetrics), &m_Channel_temp[i]))) {
            DebugPrint(cf_sparkplug_error);
        }
    }
}


/**
 * @brief Updates the NTP object's state, which will periodically sync time
 * with the NTP server.
 *
 * @return true if a sync event occurred TODO: verify this
 * @return false if we failed to sync
 */

bool update_ntp(void){
    ntp.update();
    return ntp.updated();
}

/**
 * @brief Sets the name of topics and the device ID based on the module ID
 * read from the jumpers.
 */
void generateNames(int hardware_id){
    char dev_id = hardware_id + '0';
    node_id.replace(NODE_ID_TOKEN, dev_id);
    nodeBirthTopic.replace(NODE_ID_TOKEN, dev_id);
    nodeDeathTopic.replace(NODE_ID_TOKEN, dev_id);
    nodeDataTopic.replace(NODE_ID_TOKEN, dev_id);
    nodeCmdTopic.replace(NODE_ID_TOKEN, dev_id);
}

/**
 * @brief Set up the arrays holding the separate birth/death sequence number
 * metrics for each broker.
 */
void setup_bdseq_metrics(void){
    for(int br_idx = 0; br_idx < NUM_BROKERS; br_idx++){
        // Copy the template metric data to the array for this broker
        memcpy(&bdseqMetrics[br_idx], &bdseqMetricsTemplate, sizeof(bdseqMetricsTemplate));

        // Set the variable pointer for the metric
        set_metric_variable(ARRAY_AND_SIZE(bdseqMetrics[br_idx]), NMA_bdSeq, &m_bdSeq[br_idx]);

        // Reset the sequence number so it starts at zero when incremented
        m_bdSeq[br_idx] = (uint64_t) -1;
    }
}

/**
 * @brief Initializes the network, sets up and checks the metric arrays, assigns
 * the IP and MAC addresses based on hardware ID jumpers, connects to NTP, and
 * sets up the broker objects.
 *
 * @return true on success
 * @return false if there's some failure
 */
bool network_init(void){
    // Set up the metrics arrays holding the node birth/death sequence numbers
    setup_bdseq_metrics();

    // We need to send at least the node metrics plus bdseq
    set_max_metrics(NUM_ELEM(bdseqMetrics[0]) + NUM_ELEM(NodeMetrics));

    // Check that the alias numbers in the metrics are valid and unique
    for(int i = 0; i < NUM_BROKERS; ++i)
        if(!check_metrics(ARRAY_AND_SIZE(bdseqMetrics[i]), NMA_bdSeq + 1)){
            DebugPrint(cf_sparkplug_error);
            return false;
        }
    if(!check_metrics(ARRAY_AND_SIZE(NodeMetrics     ), EndNodeMetricAlias     )){
        DebugPrint(cf_sparkplug_error);
        return false;
    }

    // Point to our function for getting timestamps
    set_gettimestamp_callback(get_current_time_millis);

    IPAddress ip = TEC0_IP;      // This device's IP
    IPAddress dns(DNS);          // DNS server
    IPAddress gateway(GATEWAY);  // Network gateway
    IPAddress subnet(SUBNET);    // Network subnet

    byte mac[6] = TEC0_MAC;      // This device's MAC address

    // Adjust network addresses based on module ID
    int hardware_id = get_hardware_id();
    if(hardware_id < 0 || hardware_id > MAX_BOARD_ID){
        DebugPrintNoEOL("Invalid hardware ID ");
        DebugPrint(hardware_id);
        return false;
    }
    ip[3]  += hardware_id;
    mac[5] += hardware_id;

    //Generate the MQTT topic names
    generateNames(hardware_id);

    Ethernet.begin(mac, ip, dns, gateway, subnet);
    if(Ethernet.hardwareStatus() == EthernetNoHardware){
        DebugPrint("Ethernet Shield is not connected");
        return false;
    }
    if(Ethernet.linkStatus() == LinkOFF){
        DebugPrint("Ethernet cable is unplugged");
        // This is not a fatal error
    }

    DebugPrintNoEOL("My IP address: ");
    DebugPrint(ip);

    // These should only get called once
    ntp.setUpdateInterval(SECS_IN_HR);
    ntp.begin();
    if(!ntp.updated()){
        DebugPrintNoEOL("Trying NTP update from ");
        DebugPrintNoEOL(ntpIP);
        DebugPrintNoEOL("... ");
        ntp.forceUpdate();
    }
    if(ntp.updated()){
        DebugPrintNoEOL("NTP updated.  Time is ");
        DebugPrint(ntp.getFormattedTime());
    }
    else {
        DebugPrint("NTP not updated");
    }
    for(int i = 0; i < NUM_BROKERS; ++i) {
        m_broker[i].setClient(enet[i]);
    }
    m_broker[0].setServer(IPAddress(MQTT_BROKER1), MQTT_BROKER1_PORT);

    for(int i = 0; i < NUM_BROKERS; ++i){
        m_broker[i].setCallback(callback_worker);
        m_broker[i].setBufferSize(BIN_BUF_SIZE);
    }

    // Network has been set up successfully
    return true;
}

/**
 * @brief Check each broker is connected, and if not then attempt to connect to
 * it.  Keep the connection to any connected brokers open, process incoming MQTT
 * messages, and publish birth and data messages as necessary.  This function
 * should be called periodically.
 */
void check_brokers(void){
    // Try to connect to any brokers that aren't currently connected
    bool new_connection = false;
    for(int i = 0; i < NUM_BROKERS; ++i){
        PubSubClient *broker = &m_broker[i];
        if(!broker->connected()){
            // Try to connect to the broker
            if(!connect_to_broker(broker, i)){
                // Can't connect - ignore this broker
                continue;
            }
            new_connection = true;
            DebugPrintNoEOL("Connected to broker");
            DebugPrint(i+1);
        }
    }

    // If we made a new connection to a broker, publish our birth messages to
    // all connected brokers.  Note that this must be done before handling any
    // incoming messages.
    if(new_connection)
        publish_births();

    // Handle any incoming messages, as well as maintaining our connection to
    // the brokers
    for(int i = 0; i < NUM_BROKERS; ++i){
        PubSubClient *broker = &m_broker[i];
        if(broker->connected()) {
            broker->loop();
        }
    }

    // Have we been asked to re-publish our birth messages?
    bool rebirth = m_nodeRebirth;
    if(rebirth){
        // Don't publish birth messages if we just did that
        if(!new_connection) {
            publish_births();
        }

        // Reset the flags after publishing so that the birth message/s will
        // show which flags triggered them.  Note that an NDATA and/or DDATA
        // message will immediately follow with the flags reset to false.
        if(m_nodeRebirth) {
            m_nodeRebirth = false;
            if(!update_metric(ARRAY_AND_SIZE(NodeMetrics), &m_nodeRebirth))
                DebugPrint(cf_sparkplug_error);
        }
    }
    // Publish any Node data that has changed
    //publish_node_data();
    // Reset the next server flag if it was set
    if(m_nodeNextServer) {
        m_nodeNextServer = false;
        if(!update_metric(ARRAY_AND_SIZE(NodeMetrics), &m_nodeNextServer)) {
            DebugPrint(cf_sparkplug_error);
        }   
    }
}
