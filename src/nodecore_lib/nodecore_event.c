#include "nodecore_event.h"
#include <stdio.h>
#include <zephyr/logging/log.h>
#include "../coap_client.h"


LOG_MODULE_REGISTER(nodecore_event, LOG_LEVEL_INF);

int nodecore_send_event(const int device_ID, 
                        const char *event_type,
                        const char *severity,
                        const char *message,
                        const char *firmware_version) {
    
    int err;
    char event_data[512];

    
    int len = snprintf(
        (char *)event_data,
        sizeof(event_data),
        "{"
            "\"device_ID\":%d,"
            "\"event_type\":\"%s\","
            "\"severity\":\"%s\","
            "\"message\":\"%s\","
            "\"firmware_version\":\"%s\""
        "}",
        device_ID,
        event_type,
        severity,
        message,
        firmware_version
    );

    if (len < 0) {
        LOG_ERR("Failed to print to buffer: %d", len);
        return -1;
    }

    if (len >= sizeof(event_data)) {
        LOG_ERR("event_data data buffer too small. Needed %d bytes", len + 1);
        return -1;
    }


    size_t url_path_array_length = 2;
    const char* url_path_array[] = {"device", "event"};

    LOG_INF("Stored event_data JSON: %s", event_data);


    err = client_post_send((const uint8_t *)event_data, url_path_array, url_path_array_length);

    if(err != 0) {
            LOG_ERR("Error sendning device event!");
            return -1;
    }
    LOG_INF("Device status event!");
    return 0;
}