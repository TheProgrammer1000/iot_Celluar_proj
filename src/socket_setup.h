#pragma once

#include <zephyr/net/socket.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/tls_credentials.h>
#include <zephyr/random/random.h>

extern int sock;
extern struct sockaddr_storage server;

// uint16_t token
extern uint16_t next_token;

/* 
    @brief the server address if succesful stored in global variable server
    Setting up the socket as IPv4, UDP, security DTLS
*/
int server_resolve(void);

/* 
        Using the global server-varibeln that we have the address in and making a connection
        Here we also init the server socket which we will make the connection too
*/
int client_init(void);