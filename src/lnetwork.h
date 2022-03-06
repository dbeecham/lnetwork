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


#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)


static const char sqlite_schema[] =
    "begin;"
    "create table interfaces ("
        "ifid int not null,"
        "primary key (ifid)"
    ") without rowid;"
    "create table names ("
        "ifid int not null,"
        "ifname text not null unique check (length(ifname)<=16),"
        "primary key (ifid),"
        "foreign key (ifid) references interfaces(ifid) on delete cascade on update cascade"
    ") without rowid;"
    "create table hwaddr ("
        "ifid int not null,"
        "hwaddr text not null unique check (length(hwaddr)<=64) check (length(hwaddr)>0),"
        "primary key (ifid),"
        "foreign key (ifid) references interfaces(ifid) on delete cascade on update cascade"
    ") without rowid;"
    "create table ipv6 ("
        "ifid int not null,"
        "ipv6addr text not null unique check (length(ipv6addr)<=" STR(INET6_ADDRSTRLEN) "),"
        "primary key (ifid, ipv6addr),"
        "foreign key (ifid) references interfaces(ifid) on delete cascade on update cascade"
    ") without rowid;"
    "create table ipv4 ("
        "ifid int not null,"
        "ipv4addr text not null unique check (length(ipv4addr)<=" STR(INET_ADDRSTRLEN) "),"
        "primary key (ifid, ipv4addr),"
        "foreign key (ifid) references interfaces(ifid) on delete cascade on update cascade"
    ") without rowid;"
    "create table mtu ("
        "ifid int not null,"
        "mtu int not null,"
        "primary key (ifid),"
        "foreign key (ifid) references interfaces(ifid) on delete cascade on update cascade"
    ") without rowid;"
    "create table txqlen ("
        "ifid int not null,"
        "txqlen int not null,"
        "primary key (ifid),"
        "foreign key (ifid) references interfaces(ifid) on delete cascade on update cascade"
    ") without rowid;"
    "create table promisuous ("
        "ifid int not null,"
        "primary key (ifid),"
        "foreign key (ifid) references interfaces(ifid) on delete cascade on update cascade"
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
    bool queried_link;
    bool queried_addr;
};
