cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(iot_Celluar_proj)

target_sources(app 
    PRIVATE 
        src/main.c
        src/socket_setup.c
        src/coap_client.c
        src/modem_lte.c
        src/gnss.c
    )
