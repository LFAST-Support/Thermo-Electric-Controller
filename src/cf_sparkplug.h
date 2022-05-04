/**
 * @file cf_sparkplug.h
 * @author Adrian Loeff
 * @brief High-level Sparkplug types and functions for the Casting Furnace.
 * @version 1.0
 * @date 2022-03-11
 *
 * @copyright Copyright (c) 2022
 */

#ifndef CF_SPARKPLUG_H
#define CF_SPARKPLUG_H


#include <PubSubClient.h>
#include <sparkplugb_arduino.hpp>


// Sparkplug settings
#define HOST_ID               "Control"         // Primary Host identifier
#define HOST_STATE_TOPIC      "STATE/" HOST_ID  // Primary Host state message
#define HOST_ONLINE           "ONLINE"          // Primary Host is online
#define HOST_OFFLINE          "OFFLINE"         // Primary Host is offline

#define SPARKPLUG_VERSION     "spBv1.0"         // Sparkplug version we're using
#define NBIRTH_MESSAGE_TYPE   "NBIRTH"          // Node birth message identifier
#define DBIRTH_MESSAGE_TYPE   "DBIRTH"          // Device birth message identifier
#define NDEATH_MESSAGE_TYPE   "NDEATH"          // Node death message identifier
#define DDEATH_MESSAGE_TYPE   "DDEATH"          // Device death message identifier
#define NDATA_MESSAGE_TYPE    "NDATA"           // Node data message identifier
#define DDATA_MESSAGE_TYPE    "DDATA"           // Device data message identifier
#define NCMD_MESSAGE_TYPE     "NCMD"            // Node command message identifier
#define DCMD_MESSAGE_TYPE     "DCMD"            // Device command message identifier

#define BIN_BUF_SIZE  2048  // Binary data buffer size for Sparkplug

#define NODE_TOPIC(type, node_id)               SPARKPLUG_VERSION "/" GROUP_ID "/" type "/" node_id
#define DEVICE_TOPIC(type, node_id, device_id)  SPARKPLUG_VERSION "/" GROUP_ID "/" type "/" node_id "/" device_id

#define NUM_ELEM(array)        (sizeof(array) / sizeof(*array))
#define ARRAY_AND_SIZE(array)  (array), NUM_ELEM(array)


// Short-form type names for readability
typedef org_eclipse_tahu_protobuf_Payload         Payload;
typedef org_eclipse_tahu_protobuf_Payload_Metric  Metric;

// This structure stores the specification for a metric
typedef struct
{
    const char   *name;
    unsigned int  alias;
    bool          writable;
    uint32_t      datatype;
    void         *variable;
    bool          updated;
    unsigned long long timestamp;
} MetricSpec;


typedef unsigned long long (*GetTimestamp)(void);


// Module error message, set when an error occurs
#define MAX_CF_SPARKPLUG_ERROR_LEN  200
extern char cf_sparkplug_error[MAX_CF_SPARKPLUG_ERROR_LEN];


// Public functions

// Set the callback function to get the timestamp for a payload or metric.
void set_gettimestamp_callback(GetTimestamp timestamp_function);

// Set the maximum number of metrics that will ever need to be sent in a single
// payload.
void set_max_metrics(unsigned int max_metrics);

// Assign the specified variable pointer to the metric in the array with the
// specified alias.  Returns false if no such metric exists or if the variable
// pointer is null.
bool set_metric_variable(MetricSpec *metrics, int num_metrics,
                         unsigned int alias, void *variable);

// Make sure all the metrics in the given array have unique alias numbers in
// the given range, have non-empty names, and have been linked to variables.
// Also, if necessary increase the maximum number of metrics that can be sent
// in a single payload to the number of metrics in this array.
bool check_metrics(MetricSpec *metrics, int num_metrics, unsigned int end_alias);

// Return a pointer to the metric in the array with the specified alias.
// Returns NULL if no such metric exists.
MetricSpec * find_metric_by_alias(MetricSpec *metrics, int num_metrics,
                                  unsigned int alias);

// Return a pointer to the metric in the array with the specified variable.
// Returns NULL if no such metric exists or if the variable pointer is null.
MetricSpec * find_metric_by_variable(MetricSpec *metrics, int num_metrics,
                                     void *variable);

// Return a pointer to the metric in the array that matches the received metric.
// If the name is supplied, it is used to find a match.  Otherwise the alias is
// used to find a match.  Returns NULL if no such metric exists, if the data
// type doesn't match, or if the metric is read-only.
MetricSpec * find_received_metric(MetricSpec *metrics, int num_metrics, Metric *metric);

// Mark the metric with the specified variable as updated.  This also sets its
// timestamp.  Returns false if the metric can't be found; otherwise returns
// true.
bool update_metric(MetricSpec *metrics, int num_metrics, void *variable);

// Connect to the specified broker with the specified node ID and will topic
// using the current module payload.  Returns true if successful, or false if
// an error occurs.
bool connect(PubSubClient *broker, const char *nodeId, const char *willTopic);

// Disconnect from the current broker.  If finalTopic is specified, a final
// message will be published before disconnecting using the specified topic and
// the current module payload.
void disconnect(PubSubClient *broker, const char *finalTopic);

// Set up the module payload with default settings but no metrics.
void set_up_next_payload(void);

// Set up the module payload for an NBIRTH message, with no metrics yet.
void set_up_nbirth_payload(void);

// Set up the module payload for an NDEATH message, with no metrics yet.
void set_up_ndeath_payload(void);

// Add the metric with the specified alias or variable to the module payload.
// If variable is non-NULL then it is used to locate the matching metric.  If
// variable is NULL then the alias is used to locate the matching metric.  If
// full is false, the metric is only added if it has been updated; if full is
// true the metric is added regardless and its name is included.  Returns false
// if an error occurs; otherwise returns true.
bool add_metric(bool full, MetricSpec *metrics, int num_metrics, void *variable,
                unsigned int alias);

// Add any updated metrics in the array to the module payload.  If full is true
// include all the metrics, whether updated or not, together with their names.
// Returns false if an error occurs; otherwise returns true.
bool add_metrics(bool full, MetricSpec *metrics, int num_metrics);

// Publish the module payload with the specified topic to all the brokers.
// Doesn't publish to brokers that we're not connected to or if the payload has
// no metrics.  Note that this sends a duplicate of the message to each broker,
// so the seq and timestamp fields will be identical.  Returns true if it
// successfully published to at least one broker; otherwise, returns false.
bool publish_payload(PubSubClient *broker_array, int num_brokers, const char *topic);

// Add the specified metrics to the module payload and publish it.  This
// function combines the add_metrics() function and the publish_payload()
// function.  Returns true if it successfully published to at least one broker;
// otherwise, returns false.
bool publish_metrics(PubSubClient *broker_array, int num_brokers, const char *topic,
                     bool full, MetricSpec *metrics, int num_metrics);

// Check to see if a received message is a Primary Host state message.  If it
// is, handle it and return true, even if it's invalid; otherwise return false.
bool process_host_state_message(const char *topic, byte *payload, unsigned int len,
                                bool *host_online);


#endif
