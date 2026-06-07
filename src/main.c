#include <stdio.h>
#include <ncs_version.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <dk_buttons_and_leds.h>
#include "./nodecore_lib/nodecore_event.h"

#include <zephyr/random/random.h>
#include <zephyr/net/coap.h>
#include "socket_setup.h"
#include "coap_client.h"
#include "modem_lte.h"
#include "app_state.h"
#include "gnss.h"
#include "adc_input.h"

// #define TX_KEEP_ALIVE_INTERVAL 6500

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

#define DEVICE_HEALTH_INTERVAL K_SECONDS(120)
#define DEVICE_LIFECYCLE_INTERVAL K_SECONDS(120)

#define DEVICE_STATUS_JSON_SIZE 256


app_state_t app_state = APP_STATE_INIT;

static uint8_t recv_buf[MESSAGE_SIZE];

static struct k_work_delayable device_health_work;
static struct k_work_delayable device_lifecycle_work;

static int32_t filtered_battery_mv = 0;

void device_health_start() {
    k_work_reschedule(&device_health_work, DEVICE_HEALTH_INTERVAL);
}

void device_lifecycle_start() {
    k_work_reschedule(&device_lifecycle_work, DEVICE_LIFECYCLE_INTERVAL);
}


void device_health_handler(struct k_work *work) {
        
        ARG_UNUSED(work);
        
        int err;

         // Läs den råa spänningen från ADC
        int32_t real_battery_mv = read_voltage_mv();

        if (real_battery_mv > 0) {
                if (filtered_battery_mv == 0) {
                // Första körningen någonsin: Sätt startvärdet direkt
                filtered_battery_mv = real_battery_mv;
                } else {
                // 2. TRÖGARE EMA-FILTER: 95% gammal spänning, 5% ny spänning
                // Detta kräver runt 15-20 mätningar för att flytta sig stort, 
                // vilket helt raderar ut spänningsfall från modemsändningar!
                filtered_battery_mv = ((filtered_battery_mv * 95) + (real_battery_mv * 5)) / 100;
                }
        }

        // Skicka in den globala, filtrerade spänningen
        uint8_t battery_percent = calculate_battery_percentage(filtered_battery_mv);
        // Valfritt: Lägg till en logg så du ser hur filtret jobbar i terminalen
        LOG_INF("Rå: %d mV | Filtrerad: %d mV -> %d%%", 
            real_battery_mv, filtered_battery_mv, battery_percent);

        size_t url_path_array_length = 2;
        const char* url_path_array[] = {"device", "health"};


        char text[DEVICE_STATUS_JSON_SIZE];

        int len = snprintf(
                text,
                sizeof(text),
                "{\"device_ID\": %d, \"battery_percent\": %d, \"firmware_version\": \"%s\"}",
                CONFIG_COAP_DEVICE_ID,
                battery_percent,
                CONFIG_FIRMARE_VERSION
        );

        if (len < 0 || len >= sizeof(text)) {
                LOG_ERR("Failed to format device status JSON");
                return;
        }

        LOG_INF("Device health payload: %s", text);
        LOG_INF("Device health payload length: %d", len);


        err = client_post_send((const uint8_t *)text, url_path_array, url_path_array_length);

        if(err != 0) {
                LOG_ERR("Error sendning device health!");
                return;
        }
        LOG_INF("Device health sended!");
        k_work_reschedule(&device_health_work, DEVICE_HEALTH_INTERVAL);
}


void device_lifecycle_handler(struct k_work *work) {
        
        ARG_UNUSED(work);
        
        int err;

        int32_t real_battery_mv = read_voltage_mv();
        uint8_t battery_percent = calculate_battery_percentage(real_battery_mv);
        // uint8_t battery_percent = 93;
                // LOG_INF("Faktisk batterispanning: %d mV (%d%%)", real_battery_mv, battery_percent);

        size_t url_path_array_length = 2;
        const char* url_path_array[] = {"device", "lifecycle"};


        char text[DEVICE_STATUS_JSON_SIZE];

        int len = snprintf(
                text,
                sizeof(text),
                "{\"device_ID\": %d, \"battery_percent\": %d, \"gnss_periodic_timeout\": %d, \"gnss_periodic_interval\": %d, \"firmware_version\": \"%s\"}",
                CONFIG_COAP_DEVICE_ID,
                battery_percent,
                CONFIG_GNSS_PERIODIC_TIMEOUT,
                CONFIG_GNSS_PERIODIC_INTERVAL,
                CONFIG_FIRMARE_VERSION
        );

        if (len < 0 || len >= sizeof(text)) {
                LOG_ERR("Failed to format device status JSON");
                return;
        }

        LOG_INF("Device lifecycle payload: %s", text);
        LOG_INF("Device lifecycle payload length: %d", len);


        err = client_post_send((const uint8_t *)text, url_path_array, url_path_array_length);

        if(err != 0) {
                LOG_ERR("Error sendning device lifecycle!");
                return;
        }
        LOG_INF("Device lifecycle sended!");
        k_work_reschedule(&device_lifecycle_work, DEVICE_LIFECYCLE_INTERVAL);
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

        err = init_adc();
        if(err < 0) {
                LOG_ERR("init adc failed!");
                return -1;
        }
        LOG_INF("adc initiazled successful!");

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
        
        nodecore_send_event(
                CONFIG_COAP_DEVICE_ID,
                "device_cellular_ready",
                "info",
                "Device cellular connection and backend communication are ready",
                "cellular",
                CONFIG_FIRMARE_VERSION
        );

        k_sleep(K_MSEC(1000));

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
                nodecore_send_event(CONFIG_COAP_DEVICE_ID, "gnss_init_failed", "error", "Failed to initialize gnss", "cellular", CONFIG_FIRMARE_VERSION);
                LOG_ERR("Failed to start GNSS");
                return 0;
        }

        nodecore_send_event(CONFIG_COAP_DEVICE_ID, "gnss_init_success", "info", "Successfully initialized gnss",  "cellular", CONFIG_FIRMARE_VERSION);

        LOG_INF("MAIN: after GNSS start");
        gnss_start_time = k_uptime_get();
 
        LOG_INF("Press button 1 on your DK to (POST) send your if got gps data");
        LOG_INF("Press button 2 on your DK to (GET) get validate data");


        // k_work_init_delayable(&device_health_work, device_health_handler);
        // k_work_submit(&device_health_work);
        // device_health_start();

        k_work_init_delayable(&device_lifecycle_work, device_lifecycle_handler);
        k_work_submit(&device_lifecycle_work);
        device_lifecycle_start();
        

//        char device_id_str[16];

//         snprintk(device_id_str, sizeof(device_id_str), "%d", CONFIG_COAP_DEVICE_ID);

//         const char *url_path_array[] = {
//                 "device",
//                 "firmware_command",
//                 device_id_str
//         };

//         if (client_get_send(url_path_array, ARRAY_SIZE(url_path_array)) != 0) {
//                 LOG_ERR("ERROR GET REQUEST");
//                 return -1;
//         }

        while(true) {

                // 1. ÄNDRING HÄR: Lägg till MSG_DONTWAIT så att recv() inte blockerar tråden
                int received = recv(sock, coap_buf, sizeof(coap_buf), MSG_DONTWAIT);

                if (received < 0) {
                        // errno == EAGAIN eller EWOULDBLOCK betyder bara att det inte fanns någon data att läsa just nu
                        if (errno != EAGAIN && errno != EWOULDBLOCK) {
                                LOG_ERR("Socket error: %d, exit", errno);
                                return -1;
                        }
                } else if (received == 0) {
                        LOG_ERR("Empty datagram");
                        return -1;
                } else {
                        // Vi tog faktiskt emot ett svar från servern! Hantera det.
                        err = client_handle_response(coap_buf, received);
                        if (err < 0) {
                                LOG_ERR("Invalid response, exit");
                                return -1;
                        }
                }
        
               // 2. Om GPS-modulen har sparat ny fix – skicka iväg den direkt!
                if(is_gnss_data_stored == true) {
                        dk_set_led_on(DK_LED1);
                        LOG_INF("POST payload: %s", gps_data);
                        LOG_INF("POST payload length: %d", strlen((char *)gps_data));
                        
                        size_t url_path_array_length = 2;
                        const char* url_path_array[] = {"sensor_data", "gps"};

                        if(client_post_send(gps_data, url_path_array, url_path_array_length) != 0) {
                                LOG_ERR("Failed to send gps data");
                                nodecore_send_event(
                                        CONFIG_COAP_DEVICE_ID,
                                        "gnss_post_failed",
                                        "error",
                                        "cellular",
                                        "Failed to send GNSS position to CoAP server",
                                        CONFIG_FIRMARE_VERSION
                                );
                        }
                        
                        // KORRIGERING 2: Vi sätter bara flaggan till false och går vidare.
                        // Serverns CoAP-svar kommer att fångas upp elegant av "MSG_DONTWAIT"-mottagaren högst upp på nästa varv!
                        is_gnss_data_stored = false;      
                }

                k_sleep(K_MSEC(50));
        }

        close(sock);
        return 0;
}
