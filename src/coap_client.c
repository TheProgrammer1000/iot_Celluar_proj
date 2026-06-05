#include "coap_client.h"

LOG_MODULE_DECLARE(app, LOG_LEVEL_INF);

// Ersätt eller lägg till vid dina globala variabler i coap_client.c:
static uint16_t last_post_token = 0;
static uint16_t last_get_token  = 0;


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

        const uint8_t json_format = COAP_CONTENT_FORMAT_TEXT_PLAIN;

        err = coap_packet_append_option(&request, COAP_OPTION_CONTENT_FORMAT, &json_format, sizeof(json_format));
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

int client_get_send(const char *url_path_array[], size_t url_path_array_length)
{
        int err;
        struct coap_packet request;
        uint8_t tx_buf[512];
        
        // --- ANVÄND GET-TOKEN HÄR ---
        last_get_token = sys_rand32_get() & 0xFFFF;

        err = coap_packet_init(
                &request,
                tx_buf,
                sizeof(tx_buf),
                APP_COAP_VERSION,
                COAP_TYPE_CON,
                sizeof(last_get_token),
                (uint8_t *)&last_get_token, // Skicka med GET-token
                COAP_METHOD_GET,
                coap_next_id()
        );

        if (err < 0) {
                LOG_ERR("Failed to create CoAP GET request: %d", err);
                return err;
        }

        LOG_INF("Building CoAP GET path:");
        for (size_t i = 0; i < url_path_array_length; i++) {
                LOG_INF("Path[%d]: %s", i, url_path_array[i]);
                err = coap_packet_append_option(
                        &request,
                        COAP_OPTION_URI_PATH,
                        (const uint8_t *)url_path_array[i],
                        strlen(url_path_array[i])
                );
                if (err < 0) return err;
        }

        LOG_INF("CoAP GET packet size: %d bytes", request.offset);
        err = send(sock, request.data, request.offset, 0);

        if (err < 0) {
                LOG_ERR("Failed to send CoAP GET request, errno: %d", errno);
                return errno;
        }

        LOG_INF("send() returned: %d bytes", err);
        LOG_INF("CoAP GET request sent: Token: 0x%04x", last_get_token);

        return 0;
}


int client_post_send(const uint8_t *payload, const char* url_path_array[],  size_t url_path_array_length) 
{
        int err;
        struct coap_packet request;
        uint8_t tx_buf[512];

        // --- ANVÄND POST-TOKEN HÄR ---
        last_post_token = sys_rand32_get() & 0xFFFF;

        err = coap_packet_init(&request, tx_buf, sizeof(tx_buf), APP_COAP_VERSION, COAP_TYPE_CON, sizeof(last_post_token), (uint8_t*)&last_post_token, COAP_METHOD_POST, coap_next_id());
        if(err < 0) {
                LOG_ERR("Failed to send CoAP request, %d", errno);
                return errno;
        }

        for(size_t i = 0; i < url_path_array_length; i++) {
             err = coap_packet_append_option(&request, COAP_OPTION_URI_PATH, (const uint8_t *)url_path_array[i], strlen(url_path_array[i]));
             if (err < 0) return err;   
        }
        
        const uint8_t json_format = COAP_CONTENT_FORMAT_APP_JSON;
        err = coap_packet_append_option(&request, COAP_OPTION_CONTENT_FORMAT, &json_format, sizeof(json_format));
        if(err < 0) return err; 

        err = coap_packet_append_payload_marker(&request);
        if(err < 0) return err;

        err = coap_packet_append_payload(&request, payload, strlen((char*)payload));
        if(err < 0) return err;

        LOG_INF("Sending payload len: %d", strlen((char *)payload));
        err = send(sock, request.data, request.offset, 0);

        if(err < 0) {
                LOG_ERR("Failed to send CoAP request, %d", errno);
                return errno;
        }

        LOG_INF("CoAP POST request sent: Token: 0x%04x", last_post_token);
        return 0;
}

int client_handle_response(uint8_t *buf, int received) 
{
        struct coap_packet reply;
        const uint8_t *payload;
        uint16_t payload_len;
        uint8_t temp_buf[256];
        uint8_t token[8];

        int err = coap_packet_parse(&reply, buf, received, NULL, 0); 
        if(err < 0) {
                LOG_ERR("Malformed response recieved: %d", err);
                return err;
        }

        // --- 1. HANTERA TOMMA MEDDELANDEN (Svar på Token 0x0000-varningen) ---
        uint8_t msg_code = coap_header_get_code(&reply);
        if (msg_code == 0) { // 0.00 Empty Message (ACK)
                LOG_INF("Received Empty CoAP ACK/Ping from server (MID: %d)", coap_header_get_id(&reply));
                return 0; // Det är bara ett nätverkskvitto, avbryt här utan fel.
        }

        // --- 2. SKICKA ACK OM SERVERNS SVAR ÄR "CON" (Svar på återutsändnings-varningarna) ---
        uint8_t msg_type = coap_header_get_type(&reply);
        if (msg_type == COAP_TYPE_CON) {
                struct coap_packet ack_pkt;
                uint8_t ack_buf[16];
                uint16_t mid = coap_header_get_id(&reply);

                // Skapa ett tomt ACK-paket med samma Message ID (mid) som servern skickade
                int ack_err = coap_packet_init(&ack_pkt, ack_buf, sizeof(ack_buf),
                                               APP_COAP_VERSION, COAP_TYPE_ACK,
                                               0, NULL, 0, mid);
                if (ack_err >= 0) {
                        send(sock, ack_pkt.data, ack_pkt.offset, 0);
                        LOG_INF("Sent CoAP ACK back to server for MID: %d", mid);
                } else {
                        LOG_ERR("Failed to initialize ACK packet: %d", ack_err);
                }
        }

        // --- 3. TOKEN-VALIDERING ---
        uint16_t token_len = coap_header_get_token(&reply, token); 
        uint16_t received_token = 0;
        if (token_len == 2) {
                received_token = (token[1] << 8) | token[0];
        }

        if (received_token != last_get_token && received_token != last_post_token) {  
                LOG_ERR("Invalid token recieved: 0x%04x (Expected GET: 0x%04x or POST: 0x%04x)", 
                        received_token, last_get_token, last_post_token);
                return 0; 
        }

        // Hämta payload
        payload = coap_packet_get_payload(&reply, &payload_len);

        if (payload_len > 0 && payload != NULL) {
                size_t copy_len = MIN(payload_len, sizeof(temp_buf) - 1);
                memcpy(temp_buf, payload, copy_len);
                temp_buf[copy_len] = '\0';
        } else {
                strcpy((char *)temp_buf, "EMPTY");
        }
        
        LOG_INF("Response payload len: %d", payload_len);
        LOG_INF("CoAP response: Code 0x%x, Payload: %s", coap_header_get_code(&reply), (char*)temp_buf);
        
        // --- 4. DIN LOGIK FÖR ATT AGERA PÅ SVARET ---
        if (received_token == last_get_token) {
                LOG_INF(">>> This is a response to our GET request!");
                bool is_modem_info = true;
                char json_buffer[512]; // Denna buffer måste vara stor nog att rymma hela JSON-texten

                
                if (strstr((char*)temp_buf, "\"command\":\"diagnostic\"") != NULL) {
                        LOG_INF("Diagnostic command found! Let's do something...");
                        // HÄR KAN DU KÖRA DIN DIAGNOSTIK-FUNKTION!
                        diagnostic_data_t diag_report = {0};

                        // En temporär buffer för att hämta rå-RSRP-strängen från modemet
                        char temp_rsrp[32];

                        if (modem_info_string_get(MODEM_INFO_RSRP, temp_rsrp, sizeof(temp_rsrp)) < 0) {
                                LOG_ERR("Failed to get RSRP info!");
                                is_modem_info = false;   
                        }
                        // Omvandla index-strängen till ett riktigt dBm-heltal
                        int rsrp_index = atoi(temp_rsrp);
                        int rsrp_dbm = rsrp_index - 140;
                        diag_report.rsrp_dbm = rsrp_dbm;

                        LOG_INF("Signalstyrka: %d dBm", rsrp_dbm);
                        
                        
                        if (modem_info_string_get(MODEM_INFO_CELLID, diag_report.cell_id, sizeof(diag_report.cell_id)) < 0) {
                                LOG_ERR("Failed to get MODEM_INFO_CELLID!");
                                 is_modem_info = false;   
                        }
                        LOG_INF("MODEM_INFO_CELLID: %s", diag_report.cell_id);

                        if (modem_info_string_get(MODEM_INFO_OPERATOR, diag_report.operator, sizeof(diag_report.operator)) < 0) {
                                LOG_ERR("Failed to get MODEM_INFO_OPERATOR!");
                                is_modem_info = false;   
                        }
                        LOG_INF("MODEM_INFO_OPERATOR: %s", diag_report.operator);

                        
                        if(is_modem_info == false) { 
                           int json_len = snprintf(json_buffer, sizeof(json_buffer),
                                "{\"device_ID\":%d,"
                                "\"command_status\":\"failed\","
                                "\"msg\":\"Failed to get modem_info\","
                                "\"payload\":null}", // Allt ligger nu i en och samma sträng, stängs först HÄR!
                                CONFIG_COAP_DEVICE_ID);

                                if (json_len < 0 || json_len >= sizeof(json_buffer)) {
                                        LOG_ERR("JSON buffer was too small!");
                                } else {
                                        LOG_INF("Generated Diagnostic JSON: %s", json_buffer);
                                }

                                // Definiera din URL path: /device/diagnostic
                                size_t url_path_array_length = 2;
                                const char* url_path_array[] = {"device", "firmware_command"};

                                // Skicka iväg paketet!
                                if (client_post_send(json_buffer, url_path_array, url_path_array_length) != 0) {
                                        LOG_ERR("Misslyckades att skicka diagnostikrapporten via CoAP POST");
                                } else {
                                        LOG_INF("Diagnostikrapporten skickad till servern!");
                                }
                                return 0;
                        }
                        else {
                                LOG_INF("MODEM_INFO_IP_ADDRESS: %s", diag_report.ip_address);

                                
                                int32_t real_battery_mv = read_voltage_mv();
                                uint8_t battery_percent = calculate_battery_percentage(real_battery_mv);

                                diag_report.battery_percent = battery_percent;

                                LOG_INF("Faktisk batterispanning: %d mV (%d%%)", real_battery_mv,  diag_report.battery_percent);


                                char json_buffer[256]; // Denna buffer måste vara stor nog att rymma hela JSON-texten

                                char json_buffer_temp[256];
                           
                                
                                

                                int temp_len = snprintf(json_buffer_temp, sizeof(json_buffer_temp),
                                        "{\"rsrp\":%d,"
                                        "\"cell_id\":\"%s\","
                                        "\"operator\":\"%s\","
                                        "\"battery\":%d}",   

                                        diag_report.rsrp_dbm,
                                        diag_report.cell_id,
                                        diag_report.operator,
                                        diag_report.battery_percent
                                );

                                // Kontrollera att steg 1 fick plats
                                if (temp_len < 0 || temp_len >= sizeof(json_buffer_temp)) {
                                        LOG_ERR("Temporary JSON buffer was too small!");
                                }
                                
                                int json_len = snprintf(json_buffer, sizeof(json_buffer),
                                        "{\"device_ID\":%d,"
                                        "\"command_status\":\"success\","
                                        "\"msg\":\"Succeeded to get modem_info from device\","
                                        "\"payload\":%s}", // %s kommer att ersättas med hela json_buffer_temp-texten
                                        CONFIG_COAP_DEVICE_ID,
                                        json_buffer_temp // ÄNDRAT: Skickar strängen, inte längden (siffran)!
                                );

                                  // Kontrollera att slutresultatet fick plats
                                if (json_len < 0 || json_len >= sizeof(json_buffer)) {
                                        LOG_ERR("Main JSON buffer was too small!");
                                } else {
                                        LOG_INF("Generated Diagnostic JSON: %s", json_buffer);
                                }

                                // Definiera din URL path: /device/diagnostic
                                size_t url_path_array_length = 2;
                                const char* url_path_array[] = {"device", "firmware_command"};

                                // Skicka iväg paketet!
                                if (client_post_send(json_buffer, url_path_array, url_path_array_length) != 0) {
                                        LOG_ERR("Misslyckades att skicka diagnostikrapporten via CoAP POST");
                                } else {
                                        LOG_INF("Diagnostikrapporten skickad till servern!");
                                }
                                return 0;

                        }  

                }
                
                last_get_token = 0; 
                
        } else if (received_token == last_post_token) {
                LOG_INF(">>> This is a response to our POST request!");
                last_post_token = 0;
        }

        return 0;
}
