#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "lnetwork_nats_parser.h"


#ifndef CONFIG_NATS_HOST
#define CONFIG_NATS_HOST "127.0.0.1"
#endif

#ifndef CONFIG_NATS_PORT
#define CONFIG_NATS_PORT "4222"
#endif

#ifndef CONFIG_NATS_CONNECT_TIMEOUT_S
#define CONFIG_NATS_CONNECT_TIMEOUT_S 3
#endif

#ifndef CONFIG_NATS_TIMEOUT_S
#define CONFIG_NATS_TIMEOUT_S 150
#endif

#define EPOLL_NUM_EVENTS 8

#define LNETWORK_SENTINEL 8081
#define LNETWORK_EPOLLFD_SENTINEL 8082

struct lnetwork_s {
    int sentinel;
    int epollfd;
    int netlinkfd;
    char hostname[64];
    uint32_t hostname_len;
    struct {
        int fd;
        int watchdogfd;
        struct lnetwork_nats_parser_s parser;
        struct addrinfo * servinfo;
        struct addrinfo * servinfo_p;
    } nats;
};
