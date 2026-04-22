#pragma once

typedef enum {
    APP_STATE_INIT,
    APP_STATE_LTE_READY,
    APP_STATE_COAP_READY,
    APP_STATE_GNSS_RUNNING,
    APP_STATE_READY
} app_state_t;

extern app_state_t app_state;
