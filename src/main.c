#include <stdio.h>
#include <ncs_version.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <dk_buttons_and_leds.h>

#include <zephyr/random/random.h>
#include <zephyr/net/coap.h>
#include "socket_setup.h"
#include "coap_client.h"
#include "modem_lte.h"
#include "app_state.h"
#include "gnss.h"

// #define TX_KEEP_ALIVE_INTERVAL 6500

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);


app_state_t app_state = APP_STATE_INIT;

static uint8_t recv_buf[MESSAGE_SIZE];

static struct k_work_delayable device_status_work;


void device_status_start() {
    k_work_reschedule(&device_status_work, K_SECONDS(20));
}

void device_status_handler(struct k_work *work) {
        
        ARG_UNUSED(work);
        
        int err;

        size_t url_path_array_length = 2;
        const char* url_path_array[] = {"device", "status"};


        char text[64];

        int len = snprintf(
                text,
                sizeof(text),
                "{\"device_ID\": %d}",
                CONFIG_COAP_DEVICE_ID
        );

        if (len < 0 || len >= sizeof(text)) {
                LOG_ERR("Failed to format device status JSON");
                return;
        }

        err = client_post_send((const uint8_t *)text, url_path_array, url_path_array_length);

        if(err != 0) {
                LOG_ERR("Error sendning device status!");
                return;
        }
        LOG_INF("Device status sended!");
        k_work_reschedule(&device_status_work, K_SECONDS(20));
}






static void button_handler(uint32_t button_state, uint32_t has_changed)
{
       if(has_changed & DK_BTN1_MSK && button_state & DK_BTN1_MSK) {
                size_t url_path_array_length = 2;
                const char* url_path_array[] = {"device", "status"};

                const char text[] = "{\"status\": 1}";

                client_post_send(text, url_path_array, url_path_array_length);
        }
        
        if(has_changed & DK_BTN2_MSK && button_state & DK_BTN2_MSK) {
               
        }

}



int main(void)
{
        int err;
        int recieved;

        if (dk_leds_init() != 0) {
		LOG_ERR("Failed to initialize the LEDs Library");
                return 0;
	}

        err = modem_configure();
        if(err) {
                LOG_ERR("Failed to configure modem");
                return 0;
        }

        if(app_state == APP_STATE_LTE_READY) {
                dk_set_led_on(DK_LED2);
        }

        err = lte_lc_func_mode_set(LTE_LC_FUNC_MODE_ACTIVATE_GNSS);
        if(err) {
                LOG_ERR("Failed to set modem mode!");
                return 0;
        }

               if(dk_buttons_init(button_handler) != 0) {
                LOG_ERR("Failted to init buttons library");
                return 0;
        }

        if(server_resolve() != 0) {
                LOG_INF("Failted to resolve server name");
                return 0;
        }

        if(client_init() != 0) {
                LOG_INF("failed to initialize client");
                return 0;
        }

        LOG_INF("setting GNSS handler");
        if(nrf_modem_gnss_event_handler_set(gnss_event_handler) != 0)
        {
                LOG_ERR("Failed to set gnns event handler!");
                return 0;
        }

        if(nrf_modem_gnss_fix_interval_set(CONFIG_GNSS_PERIODIC_INTERVAL) != 0) {
                LOG_ERR("Failed to set gnss fix interal");
                return 0;
        }

        if(nrf_modem_gnss_fix_retry_set(CONFIG_GNSS_PERIODIC_TIMEOUT) != 0) {
                LOG_ERR("Failed to set gnss retry");
                return 0;
        }

        err = agnss_init();
        if (err) {
                LOG_ERR("Failed to init A-GNSS");
                return 0;
        }

        LOG_INF("Starting GNSS");
        if(nrf_modem_gnss_start() != 0) {
                LOG_ERR("Failed to start GNSS");
                return;
        }

        LOG_INF("MAIN: after GNSS start");
        gnss_start_time = k_uptime_get();
 
        LOG_INF("Press button 1 on your DK to (POST) send your if got gps data");
        LOG_INF("Press button 2 on your DK to (GET) get validate data");


        k_work_init_delayable(&device_status_work, device_status_handler);
        k_work_submit(&device_status_work);
        device_status_start();
        
        while(true) {

    
                if(is_gnss_data_stored == true) {
                        dk_set_led_on(DK_LED1);
                        LOG_INF("POST payload: %s", gps_data);
                        LOG_INF("POST payload length: %d", strlen((char *)gps_data));
                        

                        size_t url_path_array_length = 2;
                        const char* url_path_array[] = {"sensor_data", "gps"};

                        if(client_post_send(gps_data, url_path_array, url_path_array_length) != 0) {
                                LOG_ERR("Failed to send gps data");
                                return 0;
                        }

                        // Efter vi har skickat så väntar vi här på respons från server om bekräftelse på hur det gick med senden
                        int received = recv(sock, coap_buf, sizeof(coap_buf), 0);

                        if (received < 0) {
                                LOG_ERR("Socket error: %d, exit", errno);
                                break;
                        } else if (received == 0) {
                                LOG_ERR("Empty datagram");
                                continue;
                        }

                        err = client_handle_response(coap_buf, received);
                        if (err < 0) {
                                LOG_ERR("Invalid response, exit");
                                break;
                        }
                        else {
                                is_gnss_data_stored = false;
                        }
                }
                k_sleep(K_MSEC(50));
        }

        close(sock);
        return 0;
}
