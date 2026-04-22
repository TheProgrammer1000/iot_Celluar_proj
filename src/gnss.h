#pragma once

#include <nrf_modem_gnss.h>
#include <zephyr/logging/log.h>
#include "app_state.h"

#define MESSAGE_SIZE 256
#define SSTRLEN(s) (sizeof(s) - 1)

extern struct nrf_modem_gnss_pvt_data_frame pvt_data;
extern int64_t gnss_start_time;
extern bool first_fix;
extern bool is_gps_data_stored;
extern uint8_t gps_data[MESSAGE_SIZE];


/*
    @brief Print the GNSS posistion data into the buffer
*/
void print_fix_data(struct nrf_modem_gnss_pvt_data_frame *pvt_data);

// Function to store the gps location, Accurancy and datetime on global varible gps_data
int store_gps_data(struct nrf_modem_gnss_pvt_data_frame *pvt_data);


// @brief Handling the diffrent event from the GNSS reciever
void gnss_event_handler(int event);