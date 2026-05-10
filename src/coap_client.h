#pragma once
#include <zephyr/net/coap.h>
#include <zephyr/logging/log.h>
#include "socket_setup.h"

#define APP_COAP_MAX_MSG_LEN 1280
#define APP_COAP_VERSION 1
#define MESSAGE_TO_SEND "Hi, from dennis board!!!!!"


// CoAP
static uint8_t coap_buf[APP_COAP_MAX_MSG_LEN];


int client_get_send();

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
