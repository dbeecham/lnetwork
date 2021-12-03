#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <sqlite3.h>

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


static const char sqlite_schema[] =
    "begin;"
    "create table interfaces ("
        "ifid int not null,"
        "ifname text not null unique check (length(ifname)<=16),"
        "primary key (ifid)"
    ") without rowid;"
    "create table ipv6 ("
        "ifid int not null,"
        "ifname text not null unique check (length(ifname)<=16),"
        "ipv6addr text not null unique check (length(ipv6addr)<=128),"
        "primary key (ifid, ipv6addr)"
    ") without rowid;"
    "pragma user_version = 1;"
    "commit;";


struct lnetwork_s {
    int sentinel;
    int epollfd;
    int netlinkfd;
    char hostname[64];
    uint32_t hostname_len;
    sqlite3 * db;
    struct {
        int fd;
        int watchdogfd;
        struct lnetwork_nats_parser_s parser;
        struct addrinfo * servinfo;
        struct addrinfo * servinfo_p;
    } nats;
};
