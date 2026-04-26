#include "socket_setup.h"

LOG_MODULE_DECLARE(app, LOG_LEVEL_INF);

int sock = -1;
struct sockaddr_storage server;
uint16_t next_token;

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

int client_init() {
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
    
    LOG_INF("Successfully connected to the server");
    /* Generate a random token after the socket is connected */
    next_token = sys_rand32_get();

    return 0;
}