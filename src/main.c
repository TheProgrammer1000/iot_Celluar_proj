#include <stdio.h>
#include <ncs_version.h>

#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/logging/log.h>
#include <dk_buttons_and_leds.h>

#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>

#include <nrf_modem_gnss.h>

// For PSK encryption and stuff
#include <modem/modem_key_mgmt.h>
#include <zephyr/net/tls_credentials.h>

#include <zephyr/random/random.h>
#include <zephyr/net/coap.h>

#define APP_COAP_VERSION 1
#define APP_COAP_MAX_MSG_LEN 1280
#define SEC_TAG 12

#define MESSAGE_TO_SEND "Hi dennis here from the board!!"
#define MESSAGE_SIZE 256
#define SSTRLEN(s) (sizeof(s) - 1)
#define TX_KEEP_ALIVE_INTERVAL 6500


LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

// CoAP
static uint8_t coap_buf[APP_COAP_MAX_MSG_LEN];

// uint16_t token
static uint16_t next_token;

// Socket
static int sock;
static struct sockaddr_storage server;
static uint8_t recv_buf[MESSAGE_SIZE];
static struct k_work_delayable rx_work;

static K_SEM_DEFINE(lte_connected, 0, 1);

// This will be used when reading PVT data from GNSS
static struct nrf_modem_gnss_pvt_data_frame pvt_data;
static int64_t gnss_start_time;
static bool first_fix = false;
static bool is_gps_data_stored = false;
static uint8_t gps_data[MESSAGE_SIZE];


int client_get_send();
int client_put_send();

/* Getting the server address if succesful stored in global variable server*/
int server_resolve() {
        int err;

        // Getting the actual result and the address info
        struct addrinfo* result;

        // Specifying the address info
        struct addrinfo hints = {
                .ai_family = AF_INET,
                .ai_socktype = SOCK_DGRAM, 
        };

        err = getaddrinfo(CONFIG_COAP_SERVER_HOSTNAME, NULL, &hints, &result);
        if(err != 0) {
                LOG_INF("Failed to get address info");
                return -EIO;
        }

        if(result == NULL) {
                LOG_INF("ERROR: Address not found");
                return -ENOENT;
        }

        struct sockaddr_in* server_addr = (struct sockaddr_in*)&server;
        struct sockaddr_in* addr_temp = (struct sockaddr_in*)result->ai_addr;

        server_addr->sin_addr   = addr_temp->sin_addr;
        server_addr->sin_family = addr_temp->sin_family;
        server_addr->sin_port   = htons(CONFIG_COAP_SERVER_PORT);
        
        // converting network address to string to print it out
        char ipv4_addr[NET_IPV4_ADDR_LEN];
        inet_ntop(AF_INET, &server_addr->sin_addr.s_addr, ipv4_addr, sizeof(ipv4_addr));
        LOG_INF("IPv4 Address found %s", ipv4_addr);

        freeaddrinfo(result); 
        
        return 0;
}

/* 
        Using the global server-varibeln that we have the address in and making a connection
        Here we also init the server socket which we will make the connection too
*/
int client_init() {
        int err;

        enum {
                NONE = 0,
                OPTIONAL = 1,
                REQUIRED = 2,
        };

        int verify = REQUIRED;

        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_DTLS_1_2);
        if(sock < 0) {
                LOG_INF("Connection failed : %d", errno);
                return -errno;
        }

        /* Set the TLS PEER verify*/
        err = setsockopt(sock, SOL_TLS, TLS_PEER_VERIFY, &verify, sizeof(verify));
        if (err) {
		LOG_ERR("Failed to setup peer verification, errno %d\n", errno);
		return -errno;
	}

        /*  Set the TLS hostname */
	err = setsockopt(sock, SOL_TLS, TLS_HOSTNAME, CONFIG_COAP_SERVER_HOSTNAME, strlen(CONFIG_COAP_SERVER_HOSTNAME));
	if (err) {
		LOG_ERR("Failed to setup TLS hostname (%s), errno %d\n",
			CONFIG_COAP_SERVER_HOSTNAME, errno);
		return -errno;
	}

	/* Set the credential security tag */
	sec_tag_t sec_tag_list[] = { SEC_TAG };

	err = setsockopt(sock, SOL_TLS, TLS_SEC_TAG_LIST, sec_tag_list,
			sizeof(sec_tag_t) * ARRAY_SIZE(sec_tag_list));
	if (err) {
		LOG_ERR("Failed to setup socket security tag, errno %d\n", errno);
		return -errno;
	}



        err = connect(sock, (struct sockaddr*)&server, sizeof(struct sockaddr_in));
        if(err < 0) {
                LOG_INF("Connect failed : %d", errno);
                return -errno;
        }
        
        LOG_INF("Successfully connected to the server");
        /* Generate a random token after the socket is connected */
	next_token = sys_rand32_get();

        return 0;
}



static void button_handler(uint32_t button_state, uint32_t has_changed)
{

        if(has_changed & DK_BTN1_MSK && button_state & DK_BTN1_MSK) {
                if(is_gps_data_stored == true) {
                        // LOG_INF("POST payload: %s", gps_data);
                        // LOG_INF("POST payload length: %d", strlen((char *)gps_data));
                        client_post_send();
                        is_gps_data_stored = false;
                }
                else {
                        LOG_INF("No gps data is stored!");
                }
        }
        
        if(has_changed & DK_BTN2_MSK && button_state & DK_BTN2_MSK) {
                client_get_send();
        }

}



void lte_handler(const struct lte_lc_evt *const evt) {
        switch(evt->type) {
                case LTE_LC_EVT_NW_REG_STATUS:
                        if((evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_HOME) && (evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_ROAMING)) {
                                break;
                        }

                        LOG_INF("Network registration status: %s",    
                                evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_HOME ?
                                "Connected home network" : "Connected - Roaming");

                        k_sem_give(&lte_connected);
                        break;
                case LTE_LC_EVT_RRC_UPDATE:
                        if(evt->rrc_mode == LTE_LC_RRC_MODE_CONNECTED) {
                                LOG_INF("RRC Connection state: Connected");
                        }
                        else if(evt->rrc_mode == LTE_LC_RRC_MODE_IDLE) {
                                LOG_INF("RRC Connection state: Idle");
                        }
                        break;

                case LTE_LC_EVT_PSM_UPDATE:
                        LOG_INF("PSM parameter update: TAU: %d, Active time: %d",
		                evt->psm_cfg.tau, evt->psm_cfg.active_time);
                        if (evt->psm_cfg.active_time == -1){
		                LOG_ERR("Network rejected PSM parameters. Failed to enable PSM");
	                }
                        break;
                case LTE_LC_EVT_EDRX_UPDATE:
                        LOG_INF("eDRX parameter update: eDRX: %f, PTW: %f",
                                (double)evt->edrx_cfg.edrx, (double)evt->edrx_cfg.ptw);
                        break;
                default:
                        break;
        }

}

static int modem_configure(void) {
        int err;

        LOG_INF("Init modem library");
        err = nrf_modem_lib_init();
        if(err) {
                LOG_ERR("FAILED to init modem lib, error: %d", err);
                return err;
        }

        err = modem_key_mgmt_write(SEC_TAG, MODEM_KEY_MGMT_CRED_TYPE_IDENTITY, CONFIG_COAP_DEVICE_NAME, 
			strlen(CONFIG_COAP_DEVICE_NAME));
        if (err) {
                LOG_ERR("Failed to write identity: %d\n", err);
                return err;
        }

        err = modem_key_mgmt_write(SEC_TAG, MODEM_KEY_MGMT_CRED_TYPE_PSK, CONFIG_COAP_SERVER_PSK, 
			strlen(CONFIG_COAP_SERVER_PSK));
        if (err) {
                LOG_ERR("Failed to write identity: %d\n", err);
                return err;
        }


        err = lte_lc_psm_req(true);
        if (err) {
                LOG_ERR("lte_lc_psm_req, error: %d", err);
        } 
        err = lte_lc_edrx_req(true);
        if (err) {
                LOG_ERR("lte_lc_edrx_req, error: %d", err);
        }


        LOG_INF("Connecting to LTE network");
        err = lte_lc_connect_async(lte_handler);
        if(err) {
                LOG_ERR("Error in lte_lc_connect_async, error: %d", err);
                return err;
        }

        k_sem_take(&lte_connected, K_FOREVER);
	LOG_INF("Connected to LTE network");
	dk_set_led_on(DK_LED2);

        return 0;
}


// NOTE: here if successful we will store the data in gloabal variable coap_buf
int client_get_send() {
        int err;

        struct coap_packet request;

        err = coap_packet_init(&request, coap_buf, sizeof(coap_buf), APP_COAP_VERSION, COAP_TYPE_NON_CON, sizeof(next_token), (uint8_t* )&next_token, COAP_METHOD_GET, coap_next_id());

        if(err < 0) {
                LOG_ERR("Failed to create CoAP request request, %d", err);
                return err;
        }

        err = coap_packet_append_option(&request, COAP_OPTION_URI_PATH, (uint8_t*)&CONFIG_COAP_RX_RESOURCE, strlen(CONFIG_COAP_RX_RESOURCE));
        if (err < 0) {
	        LOG_ERR("Failed to encode CoAP option, %d", err);
	        return err;
        }

        err = send(sock, request.data, request.offset, 0);
        if(err < 0) {
                LOG_ERR("Failed to send CoAP request, %d", errno);
                return errno;
        }

        LOG_INF("CoAP GET request sent: Token: 0x%04x", next_token);

        return 0;
}

// NOTE: here if successful we will store the data in gloabal variable coap_buf
int client_put_send() {
        int err;
        struct coap_packet request;

        err = coap_packet_init(&request, coap_buf, sizeof(coap_buf), APP_COAP_VERSION, COAP_TYPE_NON_CON, sizeof(next_token), (uint8_t*)&next_token, COAP_METHOD_PUT, coap_next_id());
        if(err < 0) {
                LOG_ERR("Failted to send CoAP request, %d", errno);
                return errno;
        }

        err = coap_packet_append_option(&request, COAP_OPTION_URI_PATH, (uint8_t*)&CONFIG_COAP_TX_RESOURCE, strlen(CONFIG_COAP_TX_RESOURCE));
        if(err < 0) {
                LOG_ERR("Failed to encode CoAP option, %d", err);
	        return err; 
        }

        const uint8_t text_plain = COAP_CONTENT_FORMAT_TEXT_PLAIN;

        err = coap_packet_append_option(&request, COAP_OPTION_CONTENT_FORMAT, &text_plain, sizeof(text_plain));
        if(err < 0) {
                LOG_ERR("Failed to append CoAP option, %d", err);
	        return err; 
        }

        err = coap_packet_append_payload_marker(&request);
        if(err < 0) {
                LOG_ERR("Failed to append payload marker: %d", err);
                return err;
        }

        err = coap_packet_append_payload(&request, (uint8_t*)MESSAGE_TO_SEND, sizeof(MESSAGE_TO_SEND));
        if(err < 0) {
                LOG_ERR("Failed to append payload: %d", err);
                return err;
        }

        /*
                Det är i detta request packet vi har packeterat vårt meddalnde i property data
                Sedan i offset finns längden på den 
        */
        err = send(sock, request.data, request.offset, 0);

        if(err < 0) {
                LOG_ERR("Failed to send CoAP request, %d", errno);
                return errno;
        }

        LOG_INF("CoAP PUT request sent: Token: 0x%04x", next_token);

        return 0;
}

int client_post_send() {
        int err;
        struct coap_packet request;

        err = coap_packet_init(&request, coap_buf, sizeof(coap_buf), APP_COAP_VERSION, COAP_TYPE_NON_CON, sizeof(next_token), (uint8_t*)&next_token, COAP_METHOD_POST, coap_next_id());
        if(err < 0) {
                LOG_ERR("Failted to send CoAP request, %d", errno);
                return errno;
        }

        err = coap_packet_append_option(&request, COAP_OPTION_URI_PATH, (uint8_t*)&CONFIG_COAP_TX_RESOURCE, strlen(CONFIG_COAP_TX_RESOURCE));
        if(err < 0) {
                LOG_ERR("Failed to encode CoAP option, %d", err);
	        return err; 
        }

        const uint8_t text_plain = COAP_CONTENT_FORMAT_TEXT_PLAIN;

        err = coap_packet_append_option(&request, COAP_OPTION_CONTENT_FORMAT, &text_plain, sizeof(text_plain));
        if(err < 0) {
                LOG_ERR("Failed to append CoAP option, %d", err);
	        return err; 
        }

        err = coap_packet_append_payload_marker(&request);
        if(err < 0) {
                LOG_ERR("Failed to append payload marker: %d", err);
                return err;
        }

        err = coap_packet_append_payload(&request, gps_data, strlen((char*)gps_data));
        if(err < 0) {
                LOG_ERR("Failed to append payload: %d", err);
                return err;
        }


        LOG_INF("Sending GPS payload: %s", gps_data);
        LOG_INF("Sending GPS payload len: %d", strlen((char *)gps_data));
        /*
                Det är i detta request packet vi har packeterat vårt meddalnde i property data
                Sedan i offset finns längden på den 
        */
        err = send(sock, request.data, request.offset, 0);

        if(err < 0) {
                LOG_ERR("Failed to send CoAP request, %d", errno);
                return errno;
        }

        LOG_INF("CoAP POST request sent: Token: 0x%04x", next_token);

        return 0;
}

// Here after we recv from the socket we put the the values in buf and the length of the buf into varible recieved
static int client_handle_response(uint8_t *buf, int received) {
        struct coap_packet reply;
        const uint8_t *payload;
	uint16_t payload_len;
	uint8_t temp_buf[256];
        uint8_t token[8];

        int err = coap_packet_parse(&reply, buf, received, NULL, 0); // Vi får buf som är array med det vi fick från clienten och recieved som är hur många bytes det var, vi får också CoAP packet i vår variabel "reply"
        if(err < 0) {
                LOG_ERR("Malformed responsed recieved: %d", err);
                return err;
        }

        uint16_t token_len = coap_header_get_token(&reply, token); // Här så sätter de token till det vi fick från klienten och vi får ut längden av tokens längd från clienten
        if((token_len != sizeof(next_token))  || 
           (memcmp(&next_token, token, sizeof(next_token)) != 0)) 
        {  
                // Kollar på längden på tokena också jämför värdet mellan dessa med memcmp
                LOG_ERR("Invalid token recieved: 0x%02x%02x", token[1], token[0]);
                return 0;
        }

        payload = coap_packet_get_payload(&reply, &payload_len);
        if(payload_len > 0) {
                snprintf((char*)temp_buf, MIN(payload_len+1, sizeof(temp_buf)), "%s", payload);
        } else {
                strcpy((char*)temp_buf, "EMPTY");
        }

        LOG_INF("Response payload len: %d", payload_len);

        LOG_INF("CoAP response: Code 0x%x, Token 0x%02x%02x, Payload: %s",
                coap_header_get_code(&reply), token[1], token[0], (char*)temp_buf);

        return 0;

}


static void rx_work_fn(struct k_wok *work) {
        client_get_send();
}


static void print_fix_data(struct nrf_modem_gnss_pvt_data_frame *pvt_data)
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

// Function to store the gps location, Accurancy and datetime on global varible gps_data
static int store_gps_data(struct nrf_modem_gnss_pvt_data_frame *pvt_data) {
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

static void gnss_event_handler(int event) {
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
                                dk_set_led_on(DK_LED1);
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

        LOG_INF("Starting GNSS");
        if(nrf_modem_gnss_start() != 0) {
                LOG_ERR("Failed to start GNSS");
                return;
        }
        gnss_start_time = k_uptime_get();
 
        LOG_INF("Press button 1 on your DK to (POST) send your if got gps data");
        LOG_INF("Press button 2 on your DK to (GET) get validate data");
        
        while(true) {
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
        }

        close(sock);
        return 0;
}
