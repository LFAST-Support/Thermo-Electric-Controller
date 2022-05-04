/**
 * @file cf_sparkplug.cpp
 * @author Adrian Loeff
 * @brief High-level Sparkplug types and functions for the Casting Furnace.
 * @version 1.0
 * @date 2022-03-11
 *
 * @copyright Copyright (c) 2022
 */


#include "cf_sparkplug.h"


/*
  Private variables
*/

// Module error message, set when an error occurs
char cf_sparkplug_error[MAX_CF_SPARKPLUG_ERROR_LEN] = "No error";

// Sparkplug variables
static uint8_t encode_buffer[BIN_BUF_SIZE];  // Buffer to store encoded binary data for Sparkplug

static uint8_t m_seq = 0;   // The message sequence number (wraps at 255 back to 0)

// Module-level metrics and payload for publishing messages
static unsigned int  m_max_metrics = 0;
static Metric       *m_metrics = NULL;
static Payload       m_payload = org_eclipse_tahu_protobuf_Payload_init_default;


// Default timestamp function that just returns zero.  Replace this by calling
// set_gettimestamp_callback() with a valid function.
unsigned long long null_timestamp(void){
    return 0;
}

// Pointer to callback function for getting payload and metric timestamps
static GetTimestamp m_gettimestamp = null_timestamp;


// Set the callback function for getting a payload or metric timestamp.
void set_gettimestamp_callback(GetTimestamp timestamp_function){
    if(timestamp_function != NULL)
        m_gettimestamp = timestamp_function;
}


// Set the maximum number of metrics that will ever need to be sent in a single
// payload.
void set_max_metrics(unsigned int max_metrics){
    // Adjust the size of the allocated memory to handle the specified number
    // of metrics
    m_max_metrics = max_metrics;
    if(m_max_metrics > 0)
        m_metrics = (Metric *) realloc(m_metrics, m_max_metrics * sizeof(*m_metrics));
    else{
        free(m_metrics);
        m_metrics = NULL;
    }

    // Discard any metrics from the current payload beyond the new maximum
    if(m_payload.metrics_count > m_max_metrics)
        m_payload.metrics_count = m_max_metrics;
}


// Assign the specified variable pointer to the metric in the array with the
// specified alias.  Returns false if no such metric exists or if the variable
// pointer is null.
bool set_metric_variable(MetricSpec *metrics, int num_metrics,
                         unsigned int alias, void *variable){
    // Check the parameters are valid
    if(variable == NULL){
        snprintf(cf_sparkplug_error, sizeof(cf_sparkplug_error),
                 "Null variable");
        return false;
    }

    MetricSpec *metric = find_metric_by_alias(metrics, num_metrics, alias);
    if(metric == NULL)
        // Couldn't find the specified metric
        return false;

    // Found the metric - set its variable pointer to the specified address
    metric->variable = variable;

    // Success
    return true;
}


// Make sure all the metrics in the given array have unique alias numbers in
// the given range, have non-empty names, and have been linked to variables.
// Also, if necessary increase the maximum number of metrics that can be sent
// in a single payload to the number of metrics in this array.
bool check_metrics(MetricSpec *metrics, int num_metrics, unsigned int end_alias){
    // Check the parameters are valid
    if(metrics == NULL || num_metrics <= 0){
        // Invalid metric array
        snprintf(cf_sparkplug_error, sizeof(cf_sparkplug_error),
                 "Empty metrics array");
        return false;
    }

    // Set up a temporary array to track which aliases have been used
    unsigned int first_alias = end_alias - num_metrics;
    unsigned int last_alias  = end_alias - 1;
    bool *alias_found = (bool *) calloc(num_metrics, sizeof(*alias_found));
    if(alias_found == NULL){
        snprintf(cf_sparkplug_error, sizeof(cf_sparkplug_error),
                 "No memory for %d alias_found entries", num_metrics);
        return false;
    }

    // Check each metric in the array
    for(int idx = 0; idx < num_metrics; idx++){
        MetricSpec *metric = &metrics[idx];
        if(metric->name == NULL || strcmp(metric->name, "") == 0){
            // This metric hasn't been given a valid name
            snprintf(cf_sparkplug_error, sizeof(cf_sparkplug_error),
                     "Empty name for metric #%d", idx);
            free(alias_found);
            return false;
        }
        if(metric->variable == NULL){
            // This metric hasn't been linked to a variable
            snprintf(cf_sparkplug_error, sizeof(cf_sparkplug_error),
                     "Null variable for metric #%d (%s)", idx, metric->name);
            free(alias_found);
            return false;
        }
        unsigned int alias_num = metric->alias;
        if(alias_num < first_alias || alias_num > last_alias){
            // Alias number is out of range
            snprintf(cf_sparkplug_error, sizeof(cf_sparkplug_error),
                     "Metric alias is out of range: %d [%d,%d]",
                     alias_num, first_alias, last_alias);
            free(alias_found);
            return false;
        }
        if(alias_found[alias_num - first_alias]){
            // Alias number has already been used
            snprintf(cf_sparkplug_error, sizeof(cf_sparkplug_error),
                     "Metric alias has already been used: %d", alias_num);
            free(alias_found);
            return false;
        }
        alias_found[alias_num - first_alias] = true;
    }

    free(alias_found);

    // Set aside enough memory to store at least this many metrics
    if((unsigned) num_metrics > m_max_metrics)
        set_max_metrics(num_metrics);

    // All alias numbers are valid and unique
    return true;
}


// Return a pointer to the metric in the array with the specified alias.
// Returns NULL if no such metric exists.
MetricSpec * find_metric_by_alias(MetricSpec *metrics, int num_metrics,
                                  unsigned int alias){
    // Check the parameters are valid
    if(metrics == NULL || num_metrics <= 0){
        // Invalid metric array
        snprintf(cf_sparkplug_error, sizeof(cf_sparkplug_error),
                 "Empty metrics array");
        return NULL;
    }

    for(int idx = 0; idx < num_metrics; idx++){
        if(metrics[idx].alias == alias)
            // Found it
            return &metrics[idx];
    }

    // A metric with the specified alias wasn't in the metrics array
    snprintf(cf_sparkplug_error, sizeof(cf_sparkplug_error),
             "No metric with alias %u", alias);
    return NULL;
}


// Return a pointer to the metric in the array with the specified variable.
// Returns NULL if no such metric exists or if the variable pointer is null.
MetricSpec * find_metric_by_variable(MetricSpec *metrics, int num_metrics,
                                     void *variable){
    // Check the parameters are valid
    if(metrics == NULL || num_metrics <= 0){
        // Invalid metric array
        snprintf(cf_sparkplug_error, sizeof(cf_sparkplug_error),
                 "Empty metrics array");
        return NULL;
    }
    if(variable == NULL){
        snprintf(cf_sparkplug_error, sizeof(cf_sparkplug_error),
                 "Null variable");
        return NULL;
    }

    for(int idx = 0; idx < num_metrics; idx++){
        if(metrics[idx].variable == variable)
            // Found it
            return &metrics[idx];
    }

    // A metric with the specified variable wasn't in the metrics array
    snprintf(cf_sparkplug_error, sizeof(cf_sparkplug_error),
                 "No metric for variable");
    return NULL;
}


// Return a pointer to the metric in the array that matches the received metric.
// If the name is supplied, it is used to find a match.  Otherwise the alias is
// used to find a match.  Returns NULL if no such metric exists, if the data
// type doesn't match, or if the metric is read-only.
MetricSpec * find_received_metric(MetricSpec *metrics, int num_metrics, Metric *metric){
    // Check the parameters are valid
    if(metrics == NULL || num_metrics <= 0){
        // Invalid metric array
        snprintf(cf_sparkplug_error, sizeof(cf_sparkplug_error),
                 "Empty metrics array");
        return NULL;
    }
    if(metric == NULL){
        // Invalid metric
        snprintf(cf_sparkplug_error, sizeof(cf_sparkplug_error),
                 "Null metric");
        return NULL;
    }
    if(metric->name == NULL && !metric->has_alias){
        // Invalid metric
        snprintf(cf_sparkplug_error, sizeof(cf_sparkplug_error),
                 "No name or alias for metric");
        return NULL;
    }

    bool found = false;
    int idx;
    for(idx = 0; idx < num_metrics; idx++){
        if(metric->name != NULL){
            // The name was supplied - check to see if it matches
            if(strcmp(metrics[idx].name, metric->name) == 0){
                found = true;
                break;
            }
        }
        // Only check the alias if the name wasn't supplied
        else if(metrics[idx].alias == metric->alias){
            found = true;
            break;
        }
    }

    if(!found){
        // The metric wasn't in the metrics array
        if(metric->name != NULL)
            snprintf(cf_sparkplug_error, sizeof(cf_sparkplug_error),
                     "No metric found with name \"%s\"", metric->name);
        else
            snprintf(cf_sparkplug_error, sizeof(cf_sparkplug_error),
                     "No metric found with alias %u",
                     (unsigned int) metric->alias);
        return NULL;
    }

    // Check that the data type matches
    if(metrics[idx].datatype != metric->datatype){
        // Datatype doesn't match
        snprintf(cf_sparkplug_error, sizeof(cf_sparkplug_error),
                 "Metric datatype mismatch for %s: received %u != expected %u",
                 metrics[idx].name, (unsigned int) metric->datatype,
                 (unsigned int) metrics[idx].datatype);
        return NULL;
    }

    // Check that the metric is writable
    if(!metrics[idx].writable){
        // Metric is read-only
        snprintf(cf_sparkplug_error, sizeof(cf_sparkplug_error),
                 "Metric is read-only: %s", metrics[idx].name);
        return NULL;
    }

    // Success
    return &metrics[idx];
}


// Mark the metric with the specified variable as updated.  This also sets its
// timestamp.  Returns false if the metric can't be found; otherwise returns
// true.
bool update_metric(MetricSpec *metrics, int num_metrics, void *variable){
    MetricSpec *metric = find_metric_by_variable(metrics, num_metrics, variable);
    if(metric == NULL)
        // Couldn't find the specified metric
        return false;

    // Found the metric - mark it as updated and set its timestamp to now
    metric->updated = true;
    metric->timestamp = m_gettimestamp();

    // Success
    return true;
}


// Connect to the specified broker with the specified node ID and will topic
// using the current module payload.  Returns true if successful, or false if
// an error occurs.
bool connect(PubSubClient *broker, const char *nodeId, const char *willTopic){
    if(broker == NULL){
        snprintf(cf_sparkplug_error, sizeof(cf_sparkplug_error),
                 "connect() error: NULL broker");
        return false;
    }

    // Include the current metrics list in the payload
    m_payload.metrics = m_metrics;

    // Encode the module payload to a buffer
    sparkplugb_arduino_encoder encoder;
    int msg_len = encoder.encode(&m_payload, encode_buffer, BIN_BUF_SIZE);
    //### What is an invalid value for msg_len?
    if(msg_len <= 0 || msg_len > BIN_BUF_SIZE){
        snprintf(cf_sparkplug_error, sizeof(cf_sparkplug_error),
                 "Failed to encode Will payload: %d", msg_len);
        return false;
    }

    // Try to connect to the broker, registering the will message
    if(!broker->connect(nodeId, willTopic, 0, false, encode_buffer, msg_len)){
        // Can't connect
        snprintf(cf_sparkplug_error, sizeof(cf_sparkplug_error),
                 "Broker refused connection");
        return false;
    }

    // Success
    return true;
}


// Disconnect from the current broker.  If finalTopic is specified, a final
// message will be published before disconnecting using the specified topic and
// the current module payload.
//### Disconnect gracefully (no NDEATH) or abruptly (NDEATH published after timeout)?
//### There doesn't appear to be any way in PubSubClient to break the
//    connection without sending the MQTT_DISCONNECT message, so we may
//    have to publish NDEATH explicitly.
void disconnect(PubSubClient *broker, const char *finalTopic){
    // Only disconnect if currently connected
    if(broker != NULL && broker->connected()){
        if(finalTopic != NULL)
            // Publish the final message explicitly
            publish_payload(broker, 1, finalTopic);

        // Disconnect gracefully from the broker
        broker->disconnect();
    }
}


// Set up the module payload with default settings but no metrics.
void set_up_next_payload(void){
    m_payload.has_timestamp = true;
    m_payload.timestamp = 0;      // Not assigned yet - set when publishing
    m_payload.metrics_count = 0;  // Start off with no metrics
    m_payload.has_seq = true;
    m_payload.seq = m_seq;
}


// Set up the module payload for an NBIRTH message, with no metrics yet.
void set_up_nbirth_payload(void){
    // Reset the message sequence number for the NBIRTH message
    m_seq = 0;

    // Create NBIRTH payload
    set_up_next_payload();
}


// Set up the module payload for an NDEATH message, with no metrics yet.
void set_up_ndeath_payload(void){
    // Create NDEATH payload
    set_up_next_payload();

    // The NDEATH payload has no seq number
    m_payload.has_seq = false;
    m_payload.seq = 0;
}


// Add the specified metric to the module payload.  If full is false, the
// metric is only added if it has been updated; if full is true the metric is
// added regardless and its name is included.  If the metric's timestamp is
// zero it is set now.  Returns false if an error occurs; otherwise returns
// true.
bool add_metric_to_payload(bool full, MetricSpec *metric){
    if(metric == NULL){
        // No such metric
        snprintf(cf_sparkplug_error, sizeof(cf_sparkplug_error), "No metric");
        return false;
    }

    if(metric->variable == NULL){
        // No variable has been attached to this metric
        snprintf(cf_sparkplug_error, sizeof(cf_sparkplug_error),
                 "No metric variable");
        return false;
    }

    // Add this metric if we're adding the full metric or it has been updated
    if(full || metric->updated){
        if(m_metrics == NULL){
            // No memory set aside for metrics?
            snprintf(cf_sparkplug_error, sizeof(cf_sparkplug_error),
                     "No memory for metrics");
            return false;
        }
        if(m_payload.metrics_count >= m_max_metrics){
            // Payload is already full of metrics
            snprintf(cf_sparkplug_error, sizeof(cf_sparkplug_error),
                     "Too many metrics, > %d", m_max_metrics);
            return false;
        }

        // The metric change is no longer pending
        metric->updated = false;

        // Set the metric timestamp if it hasn't been set
        if(metric->timestamp == 0)
            metric->timestamp = m_gettimestamp();

        Metric *next_metric = &m_metrics[m_payload.metrics_count];
        m_payload.metrics_count++;

        // Include the metric name if the full metric is being added
        if(full)
            next_metric->name = (char *) metric->name;
        else
            next_metric->name = NULL;
        next_metric->has_alias = true;
        next_metric->alias = metric->alias;
        next_metric->has_timestamp = true;
        next_metric->timestamp = metric->timestamp;
        next_metric->has_is_historical = false;
        next_metric->has_is_transient = false;
        next_metric->has_is_null = false;
        next_metric->has_metadata = false;
        next_metric->has_properties = false;
        next_metric->has_datatype = true;
        next_metric->datatype = metric->datatype;

        // Set data type and value based on metric type
        switch(metric->datatype){
        case METRIC_DATA_TYPE_BOOLEAN:
            next_metric->which_value = org_eclipse_tahu_protobuf_Payload_Metric_boolean_value_tag;
            next_metric->value.boolean_value = *(bool *) metric->variable;
            break;

        case METRIC_DATA_TYPE_INT64:
            next_metric->which_value = org_eclipse_tahu_protobuf_Payload_Metric_long_value_tag;
            next_metric->value.long_value = *(uint64_t *) metric->variable;
            break;

        case METRIC_DATA_TYPE_FLOAT:
            next_metric->which_value = org_eclipse_tahu_protobuf_Payload_Metric_float_value_tag;
            next_metric->value.float_value = *(float *) metric->variable;
            break;

        case METRIC_DATA_TYPE_STRING:
            next_metric->which_value = org_eclipse_tahu_protobuf_Payload_Metric_string_value_tag;
            next_metric->value.string_value = *(char **) metric->variable;
            break;

        default:
            // Unsupported type
            snprintf(cf_sparkplug_error, sizeof(cf_sparkplug_error),
                     "Unsupported metric datatype: %u",
                     (unsigned int) metric->datatype);
            m_payload.metrics_count--;
            return false;
        }
    }

    // Success
    return true;
}


// Add the metric with the specified alias or variable to the module payload.
// If variable is non-NULL then it is used to locate the matching metric.  If
// variable is NULL then the alias is used to locate the matching metric.  If
// full is false, the metric is only added if it has been updated; if full is
// true the metric is added regardless and its name is included.  Returns false
// if an error occurs; otherwise returns true.
bool add_metric(bool full, MetricSpec *metrics, int num_metrics, void *variable,
                unsigned int alias){
    MetricSpec *metric = NULL;
    if(variable != NULL)
        metric = find_metric_by_variable(metrics, num_metrics, variable);
    else
        metric = find_metric_by_alias(metrics, num_metrics, alias);
    if(metric == NULL)
        return false;
    return add_metric_to_payload(full, metric);
}


// Add any updated metrics in the array to the module payload.  If full is true
// include all the metrics, whether updated or not, together with their names.
// Returns false if an error occurs; otherwise returns true.
bool add_metrics(bool full, MetricSpec *metrics, int num_metrics){
    // Check the parameters are valid
    if(metrics == NULL || num_metrics <= 0){
        // Invalid metric array
        snprintf(cf_sparkplug_error, sizeof(cf_sparkplug_error),
                 "Empty metrics array");
        return false;
    }

    // Scan through the metrics array looking for metrics to add
    for(int idx = 0; idx < num_metrics; idx++)
        if(!add_metric_to_payload(full, &metrics[idx]))
            return false;

    // Success
    return true;
}


// Publish the module payload with the specified topic to all the brokers.
// Doesn't publish to brokers that we're not connected to or if the payload has
// no metrics.  Note that this sends a duplicate of the message to each broker,
// so the seq and timestamp fields will be identical.  Returns true if it
// successfully published to at least one broker; otherwise, returns false.
bool publish_payload(PubSubClient *broker_array, int num_brokers, const char *topic){
    // Since the function returns false if we're not connected to any brokers,
    // an empty error message indicates no error
    strcpy(cf_sparkplug_error, "");

    // Check the parameters are valid
    if(broker_array == NULL || num_brokers <= 0){
        snprintf(cf_sparkplug_error, sizeof(cf_sparkplug_error),
                 "Empty broker array");
        return false;
    }
    if(topic == NULL){
        snprintf(cf_sparkplug_error, sizeof(cf_sparkplug_error), "Null topic");
        return false;
    }

    // Include the current metrics list in the payload
    m_payload.metrics = m_metrics;

    // Don't publish if the payload doesn't contain any metrics
    if(m_payload.metrics_count == 0 || m_payload.metrics == NULL){
        snprintf(cf_sparkplug_error, sizeof(cf_sparkplug_error), "No metrics");
        return false;
    }

    // Set the payload timestamp
    unsigned long long timestamp = m_gettimestamp();
    m_payload.timestamp = timestamp;

    // Encode the payload to a buffer
    sparkplugb_arduino_encoder encoder;
    int msg_len = encoder.encode(&m_payload, encode_buffer, BIN_BUF_SIZE);

    bool published = false;
    for(int i = 0; i < num_brokers; ++i){
        PubSubClient *broker = &broker_array[i];

        // Skip this broker if we're not connected to it
        if(!broker->connected())
            continue;

        // Send the message to the broker
        if(!broker->publish(topic, encode_buffer, msg_len, false)){
            snprintf(cf_sparkplug_error, sizeof(cf_sparkplug_error),
                     "Failed to publish message to broker%d: %s", i, topic);
            continue;
        }

        // Success
        published = true;
    }

    // Increment the sequence number if the payload was published and had a
    // sequence number
    if(published && m_payload.has_seq)
        m_seq++;

    // Return true if we published to at least one broker
    return published;
}


// Add the specified metrics to the module payload and publish it.  This
// function combines the add_metrics() function and the publish_payload()
// function.  Returns true if it successfully published to at least one broker;
// otherwise, returns false.
bool publish_metrics(PubSubClient *broker_array, int num_brokers, const char *topic,
                     bool full, MetricSpec *metrics, int num_metrics){
    return add_metrics(full, metrics, num_metrics) &&
           publish_payload(broker_array, num_brokers, topic);
}


// Check to see if a received message is a Primary Host state message.  If it
// is, handle it and return true, even if it's invalid; otherwise return false.
bool process_host_state_message(const char *topic, byte *payload, unsigned int len,
                                bool *host_online){
    if(strcmp(topic, HOST_STATE_TOPIC) != 0)
        // This is not a Primary Host state message
        return false;

    // Since the function returns true or false to indicate whether this was a
    // Primary Host state message, an empty error message indicates no error
    strcpy(cf_sparkplug_error, "");

    // Check Primary Host state
    bool online = false;
    char *payload_str = (char *) payload;
    if(payload_str == NULL)
        // No state string - assume Primary Host is not online
        snprintf(cf_sparkplug_error, sizeof(cf_sparkplug_error),
                 "Null Primary Host state: NULL, len = %u", len);
    else if(strcmp(payload_str, HOST_ONLINE) == 0)
        // Primary Host is connected to this broker
        online = true;
    else if(strcmp(payload_str, HOST_OFFLINE) == 0){
        // Primary Host is not connected to this broker
    }
    else
        // Unrecognized state - assume Primary Host is not online
        snprintf(cf_sparkplug_error, sizeof(cf_sparkplug_error),
                 "Unrecognized Primary Host state: \"%.*s\"", len, payload_str);

    // Copy the online indicator back to the caller, if desired
    if(host_online != NULL)
        *host_online = online;

    // This was a Primary Host state message
    return true;
}
