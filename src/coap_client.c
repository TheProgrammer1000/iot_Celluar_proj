#include "coap_client.h"

LOG_MODULE_DECLARE(app, LOG_LEVEL_INF);

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


int client_post_send(const uint8_t *payload) {
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

        err = coap_packet_append_payload(&request, payload, strlen((char*)payload));
        if(err < 0) {
                LOG_ERR("Failed to append payload: %d", err);
                return err;
        }


        LOG_INF("Sending GPS payload: %s", payload);
        LOG_INF("Sending GPS payload len: %d", strlen((char *)payload));
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

int client_handle_response(uint8_t *buf, int received) {
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
