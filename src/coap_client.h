#pragma once
#include <zephyr/net/coap.h>
#include <zephyr/logging/log.h>
#include <modem/modem_info.h>
#include <stdlib.h>
#include "adc_input.h"
#include "socket_setup.h"

#define APP_COAP_MAX_MSG_LEN 1280
#define APP_COAP_VERSION 1
#define MESSAGE_TO_SEND "Hi, from dennis board!!!!!"

/*
 char signal_buffer[32];
                        if (modem_info_string_get(MODEM_INFO_RSRP, signal_buffer, sizeof(signal_buffer)) < 0) {
                                LOG_ERR("Failed to get RSRP info!");
                                return -1;
                        }
                        // Omvandla index-strängen till ett riktigt dBm-heltal
                        int rsrp_index = atoi(signal_buffer);
                        int rsrp_dbm = rsrp_index - 140;

                        LOG_INF("MODEM_INFO_RSRP (Rå-index): %s", signal_buffer);
                        LOG_INF("Faktisk signalstyrka: %d dBm", rsrp_dbm);
                        
                        
                        if (modem_info_string_get(MODEM_INFO_CELLID, signal_buffer, sizeof(signal_buffer)) < 0) {
                                LOG_ERR("Failed to get MODEM_INFO_CELLID!");
                                return -1;
                        }
                        LOG_INF("MODEM_INFO_CELLID: %s", signal_buffer);

                        if (modem_info_string_get(MODEM_INFO_OPERATOR, signal_buffer, sizeof(signal_buffer)) < 0) {
                                LOG_ERR("Failed to get MODEM_INFO_OPERATOR!");
                                return -1;
                        }
                        LOG_INF("MODEM_INFO_OPERATOR: %s", signal_buffer);

                        if (modem_info_string_get(MODEM_INFO_IP_ADDRESS, signal_buffer, sizeof(signal_buffer)) < 0) {
                                LOG_ERR("Failed to get MODEM_INFO_IP_ADDRESS!");
                                return -1;
                        }
                        LOG_INF("MODEM_INFO_IP_ADDRESS: %s", signal_buffer);

                        
                        int32_t real_battery_mv = read_voltage_mv();
                        uint8_t battery_percent = calculate_battery_percentage(real_battery_mv);

                        LOG_INF("Faktisk batterispanning: %d mV (%d%%)", real_battery_mv, battery_percent);
*/


typedef struct diagnostic_data_t {
    int rsrp_dbm;
    char cell_id[32];
    char operator[32];
    char ip_address[32];
    uint8_t battery_percent;
} diagnostic_data_t;

// CoAP
static uint8_t coap_buf[APP_COAP_MAX_MSG_LEN];

int client_get_send(const char* url_path_array[],  size_t url_path_array_length);

/**
 * @brief if returning 0 then successful
 * @param payload 
 * @param url_path_array 
 * @param url_path_array_length 
 * @return 
 */


int client_post_send(const uint8_t *payload, const char* url_path_array[],  size_t url_path_array_length);

// NOTE: here if successful we will store the data in gloabal variable coap_buf
int client_put_send();

// Here after we recv from the socket we put the the values in buf and the length of the buf into varible recieved
int client_handle_response(uint8_t *buf, int received);
