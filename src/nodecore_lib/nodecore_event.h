#ifndef NODECORE_EVENT_H
#define NODECORE_EVENT_H


int nodecore_send_event(const int device_ID, 
                        const char *event_type,
                        const char *severity,
                        const char *message,
                        const char *data_transport,
                        const char *firmware_version);

#endif