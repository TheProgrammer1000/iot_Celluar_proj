#include <stdio.h>
#include <ncs_version.h>

#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/logging/log.h>
#include <dk_buttons_and_leds.h>

#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>

#include <zephyr/random/random.h>
#include <zephyr/net/coap.h>

#define APP_COAP_VERSION 1


#define SERVER_HOSTNAME "udp-echo.nordicsemi.academy"
#define SERVER_PORT "2444"

#define MESSAGE_TO_SEND "Hi dennis here from the board!!"
#define MESSAGE_SIZE 256
#define SSTRLEN(s) (sizeof(s) - 1)


LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);


static int sock;
static struct sockaddr_storage server;
static uint8_t recv_buf[MESSAGE_SIZE];

static K_SEM_DEFINE(lte_connected, 0, 1);


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

        err = getaddrinfo(SERVER_HOSTNAME, SERVER_PORT, &hints, &result);
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
        server_addr->sin_port   = addr_temp->sin_port;
        
        // converting network address to string to print it out
        char ipv4_addr[NET_IPV4_ADDR_LEN];
        inet_ntop(AF_INET, &server_addr->sin_addr.s_addr, ipv4_addr, sizeof(ipv4_addr));
        LOG_INF("IPv4 Address found %s", ipv4_addr);

        freeaddrinfo(result); 
}

/* 
        Using the global server-varibeln that we have the address in and making a connection
        Here we also init the server socket which we will make the connection too
*/
int server_connect() {
        int err;

        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if(sock < 0) {
                LOG_INF("Connection failed : %d", errno);
                return -errno;
        }
        err = connect(sock, (struct sockaddr*)&server, sizeof(struct sockaddr_in));
        if(err < 0) {
                LOG_INF("Connect failed : %d", errno);
                return -errno;
        }
}



static void button_handler(uint32_t button_state, uint32_t has_changed)
{
        /* See if the buttonstate have a button1 in it?*/
        if(button_state & DK_BTN1_MSK) {
                int err = send(sock, MESSAGE_TO_SEND, SSTRLEN(MESSAGE_TO_SEND), 0);
                if(err < 0) {
                        LOG_INF("Failed to send message, %d", errno);
                        return;        
                } LOG_INF("Successfully sent message: %s", MESSAGE_TO_SEND);
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

        
        if(dk_buttons_init(button_handler) != 0) {
                LOG_ERR("Failted to init buttons library");
                return 0;
        }

        if(server_resolve() != 0) {
                LOG_INF("Failted to resolve server name");
                return 0;
        }

        if(server_connect() != 0) {
                LOG_INF("failed to initialize client");
                return 0;
        }

        LOG_INF("Press button 1 on your DK to send your message");

        while(true) {
                recieved = recv(sock, recv_buf, sizeof(recv_buf) - 1, 0);


                if(recieved < 0) {
                        LOG_ERR("Socket error: %d, exit", errno);
                        break;
                }
                
                if(recieved == 0) {
                        LOG_ERR("Empty datagram");
                        break;
                }

                recv_buf[recieved] = 0;
                LOG_INF("DATA recieved form the server: (%s)", recv_buf);
        }

        close(sock);
        return 0;
}
