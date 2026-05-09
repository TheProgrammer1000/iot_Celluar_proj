#ifndef AGNSS_H_
#define AGNSS_H_

#include <nrf_modem_gnss.h>

// init the work
int agnss_init(void);


// Storing the local variable in modem and then submitting the work to run
void agnss_request_schedule(
        const struct nrf_modem_gnss_agnss_data_frame *req
);

#endif