#include "gnss.h"

LOG_MODULE_DECLARE(app);



bool first_fix = false;
bool is_gps_data_stored = false;
uint8_t gps_data[MESSAGE_SIZE];
int64_t gnss_start_time;
struct nrf_modem_gnss_pvt_data_frame pvt_data;


void print_fix_data(struct nrf_modem_gnss_pvt_data_frame *pvt_data)
{
	LOG_INF("Latitude:       %.06f", pvt_data->latitude);
	LOG_INF("Longitude:      %.06f", pvt_data->longitude);
	LOG_INF("Altitude:       %.01f m", (double)pvt_data->altitude);
	LOG_INF("Time (UTC):     %02u:%02u:%02u.%03u",
	       pvt_data->datetime.hour,
	       pvt_data->datetime.minute,
	       pvt_data->datetime.seconds,
	       pvt_data->datetime.ms);
}

int store_gps_data(struct nrf_modem_gnss_pvt_data_frame *pvt_data) {
    int err = snprintf(
                    (char *)gps_data,
                    sizeof(gps_data),
                    "Latitude: %.06f, Longitude: %.06f, Accuracy: %.1f m, Time (UTC): %02u:%02u:%02u.%03u",
                    pvt_data->latitude,
                    pvt_data->longitude,
                    pvt_data->accuracy,
                    pvt_data->datetime.hour,
                    pvt_data->datetime.minute,
                    pvt_data->datetime.seconds,
                    pvt_data->datetime.ms
                    );	
    
    if (err < 0) {
            LOG_ERR("Failed to print to buffer: %d", err);
            return -1;
    } 
    
    return 0;                            
}

void gnss_event_handler(int event) {
    int err;

    switch(event) {
        case NRF_MODEM_GNSS_EVT_PVT:

                LOG_INF("Searching...");
                err = nrf_modem_gnss_read(&pvt_data, sizeof(pvt_data), NRF_MODEM_GNSS_DATA_PVT);
                if(err < 0) {
                        LOG_ERR("Failed reading the gnss!!");
                        return;
                }

                if(pvt_data.flags & NRF_MODEM_GNSS_PVT_FLAG_DEADLINE_MISSED) {
                        LOG_INF("GNSS blocked by LTE activity");
                }
                if(pvt_data.flags & NRF_MODEM_GNSS_PVT_FLAG_NOT_ENOUGH_WINDOW_TIME) {
                        LOG_INF("Insufficient GNSS time window");
                }

                int num_satellites = 0;
                for (int i = 0; i < 12 ; i++) {
                        if (pvt_data.sv[i].signal != 0) {
                                LOG_INF("sv: %d, cn0: %d", pvt_data.sv[i].sv, pvt_data.sv[i].cn0);
                                num_satellites++;
                        }

                }
                LOG_INF("Number of satellites: %d", num_satellites);

                if(pvt_data.flags & NRF_MODEM_GNSS_PVT_FLAG_FIX_VALID) {
                        print_fix_data(&pvt_data);

                        if(store_gps_data(&pvt_data) != 0) {
                                LOG_ERR("Failed to store GPS data to global (gps_data)");
                                return;
                        } else {
                                is_gps_data_stored = true;
                        }
                        

                        if(!first_fix) {
                                first_fix = true;
                                LOG_INF("Time to first fix: %2.1lld s", (k_uptime_get() - gnss_start_time)/1000);
                        }
                        return;
                }

                break;
        case NRF_MODEM_GNSS_EVT_PERIODIC_WAKEUP:
                LOG_INF("GNS wakeing up!");
                break;
        case NRF_MODEM_GNSS_EVT_SLEEP_AFTER_FIX:
                LOG_INF("GNSS got fix, we sleeping...");
                break;
            
    }
}

