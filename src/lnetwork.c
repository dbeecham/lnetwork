#define _DEFAULT_SOURCE

#include <asm/types.h>
#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <net/if.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "lnetwork.h"
#include "lnetwork_hostname_parser.h"
#include "lnetwork_nats_parser.h"


int pub (
    struct lnetwork_s * lnetwork,
    const uint8_t * topic,
    const uint32_t topic_len,
    const uint8_t * rt,
    const uint32_t rt_len,
    const uint8_t * payload,
    const uint32_t payload_len
)
{
    int buf_len = 0;
    int bytes_written = 0;
    char buf[65536];

    if (0 == rt_len) {
        buf_len = snprintf(buf, sizeof(buf), "PUB %.*s %d\r\n", 
                topic_len, topic, payload_len);
    } else {
        buf_len = snprintf(buf, sizeof(buf), "PUB %.*s %.*s %d\r\n", 
                topic_len, topic, rt_len, rt, payload_len);
    }

    memcpy(buf + buf_len, payload, payload_len);
    buf_len += payload_len;
    memcpy(buf + buf_len, "\r\n", 2);
    buf_len += 2;


    bytes_written = write(lnetwork->nats.fd, buf, buf_len);
    if (-1 == bytes_written) {
        syslog(LOG_ERR, "%s:%d:%s: write: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }
    if (0 == bytes_written) {
        syslog(LOG_ERR, "%s:%d:%s: connection closed", __FILE__, __LINE__, __func__);
        return -1;
    }
    if (buf_len != bytes_written) {
        syslog(LOG_ERR, "%s:%d:%s: partial write!", __FILE__, __LINE__, __func__);
        return -1;
    }

    return 0;
}


int sub (
    struct lnetwork_s * lnetwork,
    const char * topic,
    const uint32_t topic_len,
    const uint8_t sid
)
{
    int buf_len = 0;
    int bytes_written = 0;
    char buf[4096];
    
    buf_len = snprintf(buf, 4096, "SUB %.*s %d\r\n", topic_len, topic, sid);
    bytes_written = write(lnetwork->nats.fd, buf, buf_len);
    if (-1 == bytes_written) {
        syslog(LOG_ERR, "%s:%d:%s: write: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }
    if (0 == bytes_written) {
        syslog(LOG_ERR, "%s:%d:%s: connection closed", __FILE__, __LINE__, __func__);
        return -1;
    }
    if (buf_len != bytes_written) {
        syslog(LOG_ERR, "%s:%d:%s: partial write!", __FILE__, __LINE__, __func__);
        return -1;
    }

    return 0;
}


int unsub (
    struct lnetwork_s * lnetwork,
    const uint8_t sid
)
{
    int buf_len = 0;
    int bytes_written = 0;
    char buf[128];
    
    buf_len = snprintf(buf, 128, "UNSUB %d\r\n", sid);
    bytes_written = write(lnetwork->nats.fd, buf, buf_len);
    if (-1 == bytes_written) {
        syslog(LOG_ERR, "%s:%d:%s: write: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }
    if (0 == bytes_written) {
        syslog(LOG_ERR, "%s:%d:%s: connection closed", __FILE__, __LINE__, __func__);
        return -1;
    }
    if (buf_len != bytes_written) {
        syslog(LOG_ERR, "%s:%d:%s: partial write!", __FILE__, __LINE__, __func__);
        return -1;
    }

    return 0;
}


int request (
    struct lnetwork_s * lnetwork,
    const char * topic,
    const uint32_t topic_len,
    const char * rt,
    const uint32_t rt_len,
    const uint8_t sid,
    const char * payload,
    const uint32_t payload_len
)
{
    int ret = 0;

    ret = sub(lnetwork, rt, rt_len, sid);
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: sub returned -1", __FILE__, __LINE__, __func__);
        return -1;
    }

    ret = pub(lnetwork, topic, topic_len, rt, rt_len, payload, payload_len);
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: pub returned -1", __FILE__, __LINE__, __func__);
        return -1;
    }

    return 0;
}


int lnetwork_epoll_event_netlink_newlink (
    struct lnetwork_s * lnetwork,
    struct epoll_event * event,
    struct ifinfomsg * ifi,
    uint32_t ifi_len
)
{

    // this is called when an interface gets link info, but its also called
    // when link is removed from an interface. You need to check the flags to
    // see what state it's in.

    //struct rtattr * rt = IFA_RTA(

    syslog(LOG_INFO, "%s:%d:%s: interface %d just got link, type=%d, flags=%d, family=%d",
            __FILE__, __LINE__, __func__, ifi->ifi_index, ifi->ifi_type, ifi->ifi_flags, ifi->ifi_family);

    if (IFF_UP == (IFF_UP & ifi->ifi_flags)) {
        syslog(LOG_INFO, "%s:%d:%s: its up", __FILE__, __LINE__, __func__);
    }

    if (IFF_BROADCAST == (IFF_BROADCAST & ifi->ifi_flags)) {
        syslog(LOG_INFO, "%s:%d:%s: broadcast", __FILE__, __LINE__, __func__);
    }

    if (IFF_DEBUG == (IFF_DEBUG & ifi->ifi_flags)) {
        syslog(LOG_INFO, "%s:%d:%s: debug", __FILE__, __LINE__, __func__);
    }

    if (IFF_LOOPBACK == (IFF_LOOPBACK & ifi->ifi_flags)) {
        syslog(LOG_INFO, "%s:%d:%s: loopback", __FILE__, __LINE__, __func__);
    }

    if (IFF_POINTOPOINT == (IFF_POINTOPOINT & ifi->ifi_flags)) {
        syslog(LOG_INFO, "%s:%d:%s: point-to-point", __FILE__, __LINE__, __func__);
    }

    if (IFF_RUNNING == (IFF_RUNNING & ifi->ifi_flags)) {
        syslog(LOG_INFO, "%s:%d:%s: running", __FILE__, __LINE__, __func__);
    }

    if (IFF_NOARP == (IFF_NOARP & ifi->ifi_flags)) {
        syslog(LOG_INFO, "%s:%d:%s: no arp", __FILE__, __LINE__, __func__);
    }

    if (IFF_PROMISC == (IFF_PROMISC & ifi->ifi_flags)) {
        syslog(LOG_INFO, "%s:%d:%s: no arp", __FILE__, __LINE__, __func__);
    }

    if (IFF_ALLMULTI == (IFF_ALLMULTI & ifi->ifi_flags)) {
        syslog(LOG_INFO, "%s:%d:%s: allmulti", __FILE__, __LINE__, __func__);
    }

    if (IFF_MULTICAST == (IFF_MULTICAST & ifi->ifi_flags)) {
        syslog(LOG_INFO, "%s:%d:%s: multicast", __FILE__, __LINE__, __func__);
    }


    return 0;
}


int lnetwork_epoll_event_netlink_dellink (
    struct lnetwork_s * lnetwork,
    struct epoll_event * event,
    struct ifinfomsg * ifi,
    uint32_t ifi_len
)
{
    // This is called when link is removed from a device; this probably means
    // the device was removed entirely from the device (e.g. unplugged a USB
    // ethernet device, or called 'ip link del veth').

    int ret = 0;

    syslog(LOG_INFO, "%s:%d:%s: interface %d just got removed, type=%d, flags=%d, family=%d",
            __FILE__, __LINE__, __func__, ifi->ifi_index, ifi->ifi_type, ifi->ifi_flags, ifi->ifi_family);

    if (IFF_UP == (IFF_UP & ifi->ifi_flags)) {
        syslog(LOG_INFO, "%s:%d:%s: its up", __FILE__, __LINE__, __func__);
    }

    if (IFF_BROADCAST == (IFF_BROADCAST & ifi->ifi_flags)) {
        syslog(LOG_INFO, "%s:%d:%s: broadcast", __FILE__, __LINE__, __func__);
    }

    if (IFF_DEBUG == (IFF_DEBUG & ifi->ifi_flags)) {
        syslog(LOG_INFO, "%s:%d:%s: debug", __FILE__, __LINE__, __func__);
    }

    if (IFF_LOOPBACK == (IFF_LOOPBACK & ifi->ifi_flags)) {
        syslog(LOG_INFO, "%s:%d:%s: loopback", __FILE__, __LINE__, __func__);
    }

    if (IFF_POINTOPOINT == (IFF_POINTOPOINT & ifi->ifi_flags)) {
        syslog(LOG_INFO, "%s:%d:%s: point-to-point", __FILE__, __LINE__, __func__);
    }

    if (IFF_RUNNING == (IFF_RUNNING & ifi->ifi_flags)) {
        syslog(LOG_INFO, "%s:%d:%s: running", __FILE__, __LINE__, __func__);
    }

    if (IFF_NOARP == (IFF_NOARP & ifi->ifi_flags)) {
        syslog(LOG_INFO, "%s:%d:%s: no arp", __FILE__, __LINE__, __func__);
    }

    if (IFF_PROMISC == (IFF_PROMISC & ifi->ifi_flags)) {
        syslog(LOG_INFO, "%s:%d:%s: no arp", __FILE__, __LINE__, __func__);
    }

    if (IFF_ALLMULTI == (IFF_ALLMULTI & ifi->ifi_flags)) {
        syslog(LOG_INFO, "%s:%d:%s: allmulti", __FILE__, __LINE__, __func__);
    }

    if (IFF_MULTICAST == (IFF_MULTICAST & ifi->ifi_flags)) {
        syslog(LOG_INFO, "%s:%d:%s: multicast", __FILE__, __LINE__, __func__);
    }

    return 0;
}


int lnetwork_epoll_event_netlink_deladdr_ipv6 (
    struct lnetwork_s * lnetwork,
    struct epoll_event * event,
    const struct nlmsghdr * const nlmsghdr,
    const struct ifaddrmsg * const ifa,
    const uint32_t ifa_len,
    struct in6_addr * in6_addr
)
{
    char addr[INET6_ADDRSTRLEN];
    char name[IFNAMSIZ];

    // get the name of the interface
    if (NULL == if_indextoname(ifa->ifa_index, name)) {
        #warning TODO: interface has been removed if errno is ENOIO
        syslog(LOG_ERR, "%s:%d:%s: if_indextoname: %s", __FILE__, __LINE__, __func__, strerror(errno));
        //return -1;
    }

    // get ip as a printable string
    if (NULL == inet_ntop(AF_INET6, in6_addr, addr, INET6_ADDRSTRLEN)) {
        syslog(LOG_ERR, "%s:%d:%s: inet_ntop: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }

    syslog(LOG_INFO, "%s:%d:%s: interface %d (%s) just lost addr %s",
            __FILE__, __LINE__, __func__, ifa->ifa_index, name, addr);

    if (IFA_F_TEMPORARY == (IFA_F_TEMPORARY & ifa->ifa_flags)) {
        syslog(LOG_INFO, "%s:%d:%s: temporary", __FILE__, __LINE__, __func__);
    }
    if (IFA_F_PERMANENT == (IFA_F_PERMANENT & ifa->ifa_flags)) {
        syslog(LOG_INFO, "%s:%d:%s: permanent", __FILE__, __LINE__, __func__);
    }
    if (IFA_F_TENTATIVE == (IFA_F_TENTATIVE & ifa->ifa_flags)) {
        syslog(LOG_INFO, "%s:%d:%s: tentative", __FILE__, __LINE__, __func__);
    }


    if (RT_SCOPE_UNIVERSE == (RT_SCOPE_UNIVERSE & ifa->ifa_scope)) {
        syslog(LOG_INFO, "%s:%d:%s: global scope", __FILE__, __LINE__, __func__);
    }
    else if (RT_SCOPE_LINK == (RT_SCOPE_LINK & ifa->ifa_scope)) {
        syslog(LOG_INFO, "%s:%d:%s: link scope", __FILE__, __LINE__, __func__);
    }

    return 0;
}


int lnetwork_epoll_event_netlink_newaddr_ipv6 (
    struct lnetwork_s * lnetwork,
    struct epoll_event * event,
    const struct nlmsghdr * const nlmsghdr,
    const struct ifaddrmsg * const ifa,
    const uint32_t ifa_len,
    struct in6_addr * in6_addr
)
{
    int ret = 0;
    char addr[INET6_ADDRSTRLEN];
    char ifname[IFNAMSIZ];
    char payload[4096];
    int payload_len = 0;
    char topic[512];
    int topic_len = 0;
    int bytes_written = 0;
    sqlite3_stmt * stmt;

    // get the name of the interface
    if (NULL == if_indextoname(ifa->ifa_index, ifname)) {
        syslog(LOG_ERR, "%s:%d:%s: if_indextoname: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }

    // get ip as a printable string
    if (NULL == inet_ntop(AF_INET6, in6_addr, addr, INET6_ADDRSTRLEN)) {
        syslog(LOG_ERR, "%s:%d:%s: inet_ntop: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }



    // for now, just don't handle temporary addresses. TODO: create a new table
    // for temporaries, or do something smart with them.
    if (IFA_F_TEMPORARY == (IFA_F_TEMPORARY & ifa->ifa_flags)) {
        return 0;
    }



    // prepare sqlite3 statement
    const char sql[] = "insert into ipv6(ifid, ifname, ipv6addr) values (?,?,?) on conflict(ifid,ipv6addr) do nothing returning ifname, ipv6addr";
    ret = sqlite3_prepare_v3(
        /* db = */ lnetwork->db,
        /* sql = */ sql,
        /* sql_len = */ sizeof(sql),
        /* flags = */ SQLITE_PREPARE_NORMALIZE,
        /* &stmt = */ &stmt,
        /* &sql_end = */ NULL
    );
    if (SQLITE_OK != ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_prepare_v3 returned %d: %s", __FILE__, __LINE__, __func__, ret, sqlite3_errmsg(lnetwork->db));
        return -1;
    }


    // bind ifid
    ret = sqlite3_bind_int(
        /* stmt = */ stmt,
        /* index = */ 1,
        /* int = */ ifa->ifa_index
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_bind_int returned %d: %s", __FILE__, __LINE__, __func__, ret, sqlite3_errmsg(lnetwork->db));
        return -1;
    }

    // bind ifname
    ret = sqlite3_bind_text(
        /* stmt = */ stmt,
        /* index = */ 2,
        /* text = */ ifname,
        /* text_len = */ sizeof(ifname),
        /* mem_cb = */ SQLITE_STATIC
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_bind_text returned %d: %s", __FILE__, __LINE__, __func__, ret, sqlite3_errmsg(lnetwork->db));
        return -1;
    }

    // bind ipv6addr
    ret = sqlite3_bind_text(
        /* stmt = */ stmt,
        /* index = */ 3,
        /* text = */ addr,
        /* text_len = */ sizeof(addr),
        /* mem_cb = */ SQLITE_STATIC
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_bind_text returned %d: %s", __FILE__, __LINE__, __func__, ret, sqlite3_errmsg(lnetwork->db));
        return -1;
    }


    ret = sqlite3_step(stmt);
    if (SQLITE_DONE == ret) {
        // we're done - this is not a new ip address, no need to notify anyone.
        sqlite3_finalize(stmt);
        return 0;
    }
    if (SQLITE_ROW == ret) {
        // this is a new address, let's notify people.
    }


    topic_len = snprintf(topic, sizeof(topic), "host.%.*s.lnetwork.%s.out", lnetwork->hostname_len, lnetwork->hostname, ifname);
    if (-1 == topic_len) {
        syslog(LOG_ERR, "%s:%d:%s: snprintf returned -1", __FILE__, __LINE__, __func__);
        return -1;
    }



    payload_len = snprintf(payload, sizeof(payload), "{\"address\":\"%s\"", addr);
    if (-1 == payload_len) {
        syslog(LOG_ERR, "%s:%d:%s: snprintf returned -1", __FILE__, __LINE__, __func__);
        return -1;
    }


    if (IFA_F_TEMPORARY == (IFA_F_TEMPORARY & ifa->ifa_flags)) {
        bytes_written = snprintf(payload + payload_len, sizeof(payload) - payload_len, ",\"temporary\":true");
        if (-1 == bytes_written) {
            syslog(LOG_ERR, "%s:%d:%s: snprintf returned -1", __FILE__, __LINE__, __func__);
            return -1;
        }
        payload_len += bytes_written;
    }
    if (IFA_F_PERMANENT == (IFA_F_PERMANENT & ifa->ifa_flags)) {
        bytes_written = snprintf(payload + payload_len, sizeof(payload) - payload_len, ",\"permanent\":true");
        if (-1 == bytes_written) {
            syslog(LOG_ERR, "%s:%d:%s: snprintf returned -1", __FILE__, __LINE__, __func__);
            return -1;
        }
        payload_len += bytes_written;
    }
    if (IFA_F_TENTATIVE == (IFA_F_TENTATIVE & ifa->ifa_flags)) {
        bytes_written = snprintf(payload + payload_len, sizeof(payload) - payload_len, ",\"tentative\":true");
        if (-1 == bytes_written) {
            syslog(LOG_ERR, "%s:%d:%s: snprintf returned -1", __FILE__, __LINE__, __func__);
            return -1;
        }
        payload_len += bytes_written;
    }


    if (RT_SCOPE_LINK == (RT_SCOPE_LINK & ifa->ifa_scope)) {
        bytes_written = snprintf(payload + payload_len, sizeof(payload) - payload_len, ",\"scope\":\"link\"");
        if (-1 == bytes_written) {
            syslog(LOG_ERR, "%s:%d:%s: snprintf returned -1", __FILE__, __LINE__, __func__);
            return -1;
        }
        payload_len += bytes_written;
    }
    else if (RT_SCOPE_UNIVERSE == (RT_SCOPE_UNIVERSE & ifa->ifa_scope)) {
        bytes_written = snprintf(payload + payload_len, sizeof(payload) - payload_len, ",\"scope\":\"global\"");
        if (-1 == bytes_written) {
            syslog(LOG_ERR, "%s:%d:%s: snprintf returned -1", __FILE__, __LINE__, __func__);
            return -1;
        }
        payload_len += bytes_written;
    }


    bytes_written = snprintf(payload + payload_len, sizeof(payload) - payload_len, "}");
    if (-1 == bytes_written) {
        syslog(LOG_ERR, "%s:%d:%s: snprintf returned -1", __FILE__, __LINE__, __func__);
        return -1;
    }
    payload_len += bytes_written;


    ret = pub(
        /* lnetwork = */ lnetwork,
        /* topic = */ topic,
        /* topic_len = */ topic_len,
        /* rt = */ NULL,
        /* rt_len = */ 0,
        /* payload = */ payload,
        /* payload_len = */ payload_len
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: pub returned -1", __FILE__, __LINE__, __func__);
        return -1;
    }


    return 0;
}


int lnetwork_epoll_event_netlink_newaddr_ipv4 (
    struct lnetwork_s * lnetwork,
    struct epoll_event * event,
    const struct nlmsghdr * const nlmsghdr,
    const struct ifaddrmsg * const ifa,
    const uint32_t ifa_len,
    struct in_addr * in_addr
)
{
    char addr[INET_ADDRSTRLEN];
    char name[IFNAMSIZ];

    if (NULL == if_indextoname(ifa->ifa_index, name)) {
        syslog(LOG_ERR, "%s:%d:%s: if_indextoname: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }
    if (NULL == inet_ntop(AF_INET, in_addr, addr, INET_ADDRSTRLEN)) {
        syslog(LOG_ERR, "%s:%d:%s: inet_ntop: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }

    syslog(LOG_INFO, "%s:%d:%s: interface %d (%s) just got addr %s",
            __FILE__, __LINE__, __func__, ifa->ifa_index, name, addr);

    return 0;
}


int lnetwork_epoll_event_netlink_deladdr_ipv4 (
    struct lnetwork_s * lnetwork,
    struct epoll_event * event,
    const struct nlmsghdr * const nlmsghdr,
    const struct ifaddrmsg * const ifa,
    const uint32_t ifa_len,
    struct in_addr * in_addr
)
{
    char addr[INET_ADDRSTRLEN];
    char name[IFNAMSIZ];

    if (NULL == if_indextoname(ifa->ifa_index, name)) {
        #warning TODO: interface has been removed if errno is ENOIO
        syslog(LOG_ERR, "%s:%d:%s: if_indextoname: %s", __FILE__, __LINE__, __func__, strerror(errno));
        //return -1;
    }
    if (NULL == inet_ntop(AF_INET, in_addr, addr, INET_ADDRSTRLEN)) {
        syslog(LOG_ERR, "%s:%d:%s: inet_ntop: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }

    syslog(LOG_INFO, "%s:%d:%s: interface %d (%s) just lost addr %s",
            __FILE__, __LINE__, __func__, ifa->ifa_index, name, addr);

    return 0;
}


int lnetwork_epoll_event_netlink_newaddr (
    struct lnetwork_s * lnetwork,
    struct epoll_event * event,
    const struct nlmsghdr * const nlmsghdr,
    const struct ifaddrmsg * const ifa,
    uint32_t ifa_len
)
{
    int ret = 0;

    const struct rtattr * rtattr = IFA_RTA(ifa);
    if (!RTA_OK(rtattr, ifa_len)) {
        syslog(LOG_ERR, "%s:%d:%s: RTA_OK(rtattr, rtattr_len) returned false", __FILE__, __LINE__, __func__);
        return -1;
    }


    int i = 0;
    while (1) {

        // Dispatch on rta_type

        if (IFA_LOCAL == rtattr->rta_type) {
            syslog(LOG_INFO, "%s:%d:%s: local", __FILE__, __LINE__, __func__);
        }

        else if (IFA_ADDRESS == rtattr->rta_type && AF_INET6 == ifa->ifa_family) {
            ret = lnetwork_epoll_event_netlink_newaddr_ipv6(
                /* lnetwork = */ lnetwork,
                /* epoll event = */ event,
                /* nlmsghdr = */ nlmsghdr,
                /* ifaddrmsg = */ ifa,
                /* ifaddrmsg_len = */ ifa_len,
                /* in6_addr = */ RTA_DATA(rtattr)
            );
            if (-1 == ret) {
                syslog(LOG_ERR, "%s:%d:%s: lnetwork_epoll_event_netlink_newaddr_ipv6 returned -1", __FILE__, __LINE__, __func__);
                return -1;
            }
        }

        else if (IFA_ADDRESS == rtattr->rta_type && AF_INET == ifa->ifa_family) {
            ret = lnetwork_epoll_event_netlink_newaddr_ipv4(
                /* lnetwork = */ lnetwork,
                /* epoll event = */ event,
                /* nlmsghdr = */ nlmsghdr,
                /* ifaddrmsg = */ ifa,
                /* ifaddrmsg_len = */ ifa_len,
                /* in6_addr = */ RTA_DATA(rtattr)
            );
            if (-1 == ret) {
                syslog(LOG_ERR, "%s:%d:%s: lnetwork_epoll_event_netlink_newaddr_ipv6 returned -1", __FILE__, __LINE__, __func__);
                return -1;
            }
        }

        else if (IFA_LABEL == rtattr->rta_type) {
            syslog(LOG_INFO, "%s:%d:%s: label", __FILE__, __LINE__, __func__);
        }

        else if (IFA_BROADCAST == rtattr->rta_type) {
            syslog(LOG_INFO, "%s:%d:%s: broadcast addr", __FILE__, __LINE__, __func__);
        }

        else if (IFA_ANYCAST == rtattr->rta_type) {
            syslog(LOG_INFO, "%s:%d:%s: anycast addr", __FILE__, __LINE__, __func__);
        }


        // ok fetch the next rt attribute
        rtattr = RTA_NEXT(rtattr, ifa_len);
        if (!RTA_OK(rtattr, ifa_len)) {
            break;
        }

        if (1024 < ++i) {
            syslog(LOG_ERR, "%s:%d:%s: infinite loop", __FILE__, __LINE__, __func__);
            return -1;
        }
    }

    return 0;
}


int lnetwork_epoll_event_netlink_deladdr (
    struct lnetwork_s * lnetwork,
    struct epoll_event * event,
    struct nlmsghdr * nlmsghdr,
    const struct ifaddrmsg * const ifa,
    uint32_t ifa_len
)
{
    int ret = 0;

    const struct rtattr * rtattr = IFA_RTA(ifa);
    if (!RTA_OK(rtattr, ifa_len)) {
        syslog(LOG_ERR, "%s:%d:%s: RTA_OK(rtattr, rtattr_len) returned false", __FILE__, __LINE__, __func__);
        return -1;
    }


    int i = 0;
    while (1) {

        if (IFA_LOCAL == rtattr->rta_type) {
            syslog(LOG_INFO, "%s:%d:%s: local", __FILE__, __LINE__, __func__);
        }

        else if (IFA_ADDRESS == rtattr->rta_type && AF_INET6 == ifa->ifa_family) {

            ret = lnetwork_epoll_event_netlink_deladdr_ipv6(
                /* lnetwork = */ lnetwork,
                /* epoll event = */ event,
                /* nlmsghdr = */ nlmsghdr,
                /* ifaddrmsg = */ ifa,
                /* ifaddrmsg_len = */ ifa_len,
                /* in6_addr = */ RTA_DATA(rtattr)
            );
            if (-1 == ret) {
                syslog(LOG_ERR, "%s:%d:%s: lnetwork_epoll_event_netlink_deladdr_ipv6 returned -1", __FILE__, __LINE__, __func__);
                return -1;
            }
        }

        else if (IFA_ADDRESS == rtattr->rta_type && AF_INET == ifa->ifa_family) {
            ret = lnetwork_epoll_event_netlink_deladdr_ipv4(
                /* lnetwork = */ lnetwork,
                /* epoll event = */ event,
                /* nlmsghdr = */ nlmsghdr,
                /* ifaddrmsg = */ ifa,
                /* ifaddrmsg_len = */ ifa_len,
                /* in6_addr = */ RTA_DATA(rtattr)
            );
            if (-1 == ret) {
                syslog(LOG_ERR, "%s:%d:%s: lnetwork_epoll_event_netlink_newaddr_ipv6 returned -1", __FILE__, __LINE__, __func__);
                return -1;
            }
        }

        else if (IFA_LABEL == rtattr->rta_type) {
            syslog(LOG_INFO, "%s:%d:%s: label", __FILE__, __LINE__, __func__);
        }

        else if (IFA_BROADCAST == rtattr->rta_type) {
            syslog(LOG_INFO, "%s:%d:%s: broadcast addr", __FILE__, __LINE__, __func__);
        }

        else if (IFA_ANYCAST == rtattr->rta_type) {
            syslog(LOG_INFO, "%s:%d:%s: anycast addr", __FILE__, __LINE__, __func__);
        }


        // ok fetch the next rt attribute
        rtattr = RTA_NEXT(rtattr, ifa_len);
        if (!RTA_OK(rtattr, ifa_len)) {
            break;
        }

        if (1024 < ++i) {
            syslog(LOG_ERR, "%s:%d:%s: infinite loop", __FILE__, __LINE__, __func__);
            return -1;
        }
    }

    return 0;
}


int lnetwork_epoll_event_netlink (
    struct lnetwork_s * lnetwork,
    struct epoll_event * event
)
{
    int ret = 0;
    uint8_t buf[4096];
    int bytes_read = 0;
    struct nlmsghdr * nlmsghdr;


    bytes_read = read(lnetwork->netlinkfd, buf, sizeof(buf));
    if (-1 == bytes_read) {
        syslog(LOG_ERR, "%s:%d:%s: read: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }
    if (0 == bytes_read) {
        syslog(LOG_ERR, "%s:%d:%s: read 0 bytes", __FILE__, __LINE__, __func__);
        return -1;
    }

    nlmsghdr = (struct nlmsghdr *)buf;
    if (!NLMSG_OK(nlmsghdr, bytes_read)) {
        syslog(LOG_ERR, "%s:%d:%s: NLMSG_OK(nlmsghdr) returned false", __FILE__, __LINE__, __func__);
        return -1;
    }


    int i = 0;
    while (1) {

        // dispatch on netlink message type
        switch (nlmsghdr->nlmsg_type) {
            case RTM_NEWLINK:
                ret = lnetwork_epoll_event_netlink_newlink(
                    lnetwork,
                    event,
                    (struct ifinfomsg*)NLMSG_DATA(nlmsghdr),
                    NLMSG_PAYLOAD(nlmsghdr, sizeof(struct ifinfomsg))
                );
                if (-1 == ret) {
                    syslog(LOG_ERR, "%s:%d:%s: lnetwork_epoll_event_netlink_newlink returned -1",
                            __FILE__, __LINE__, __func__);
                    return -1;
                }
                break;


            case RTM_DELLINK:
                ret = lnetwork_epoll_event_netlink_dellink(
                    lnetwork,
                    event,
                    (struct ifinfomsg*)NLMSG_DATA(nlmsghdr),
                    NLMSG_PAYLOAD(nlmsghdr, sizeof(struct ifinfomsg))
                );
                if (-1 == ret) {
                    syslog(LOG_ERR, "%s:%d:%s: lnetwork_epoll_event_netlink_dellink returned -1", __FILE__, __LINE__, __func__);
                    return -1;
                }
                break;

            case RTM_NEWADDR:
                ret = lnetwork_epoll_event_netlink_newaddr(
                    /* lnetwork = */ lnetwork,
                    /* epoll event = */ event,
                    /* nlmsghdr = */ nlmsghdr,
                    /* ifaddrmsg = */ (const struct ifaddrmsg * const)NLMSG_DATA(nlmsghdr),
                    /* ifaddrmsg_len = */ NLMSG_PAYLOAD(nlmsghdr, sizeof(struct ifaddrmsg))
                );
                if (-1 == ret) {
                    syslog(LOG_ERR, "%s:%d:%s: lnetwork_epoll_event_netlink_newaddr returned -1", __FILE__, __LINE__, __func__);
                    return -1;
                }
                break;

            case RTM_DELADDR:
                ret = lnetwork_epoll_event_netlink_deladdr(
                    /* lnetwork = */ lnetwork,
                    /* epoll event = */ event,
                    /* nlmsghdr = */ nlmsghdr,
                    /* ifaddrmsg = */ (const struct ifaddrmsg * const)NLMSG_DATA(nlmsghdr),
                    /* ifaddrmsg_len = */ NLMSG_PAYLOAD(nlmsghdr, sizeof(struct ifaddrmsg))
                );
                if (-1 == ret) {
                    syslog(LOG_ERR, "%s:%d:%s: lnetwork_epoll_event_netlink_deladdr returned -1", __FILE__, __LINE__, __func__);
                    return -1;
                }
                break;


            default:
                syslog(LOG_ERR, "%s:%d:%s: no match on netlink message type %d", __FILE__, __LINE__, __func__, nlmsghdr->nlmsg_type);
                return -1;
        }


        // get the next nlmsghdr packet
        nlmsghdr = NLMSG_NEXT(nlmsghdr, bytes_read);
        if (!NLMSG_OK(nlmsghdr, bytes_read)) {
            break;
        }
        if (NLMSG_DONE == nlmsghdr->nlmsg_type) {
            break;
        }
        if (NLMSG_ERROR == nlmsghdr->nlmsg_type) {
            syslog(LOG_ERR, "%s:%d:%s: NLMSG_ERROR == nlmsghdr->nlmsg_type", __FILE__, __LINE__, __func__);
            return -1;
        }

        // loop around, but check for infinite loops.
        if (1024 < ++i) {
            syslog(LOG_ERR, "%s:%d:%s: infinite loop", __FILE__, __LINE__, __func__);
            return -1;
        }
    }


    // Ok, we've read all messages from the netlink now; let's rearm the fd on epoll.
    ret = epoll_ctl(
        lnetwork->epollfd,
        EPOLL_CTL_MOD,
        lnetwork->netlinkfd,
        &(struct epoll_event){
            .events = EPOLLIN | EPOLLONESHOT,
            .data = event->data
        }
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: epoll_ctl: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }

    return 0;
}


int lnetwork_netlink_start (
    struct lnetwork_s * lnetwork
)
{

    int ret = 0;

    // Open a netlink socket to receive link, ip address event notifications
    lnetwork->netlinkfd = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (-1 == lnetwork->netlinkfd) {
        syslog(LOG_ERR, "%s:%d:%s: socket: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }

    // bind source address to socket
    struct sockaddr_nl sa = {
        .nl_family = AF_NETLINK,
        .nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR | RTMGRP_IPV6_IFADDR
    };

    ret = bind(lnetwork->netlinkfd, (struct sockaddr *)&sa, sizeof(sa));
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: bind: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }

    // add it to epoll
    ret = epoll_ctl(
        lnetwork->epollfd,
        EPOLL_CTL_ADD,
        lnetwork->netlinkfd,
        &(struct epoll_event){
            .events = EPOLLIN | EPOLLONESHOT,
            .data = {
                .fd = lnetwork->netlinkfd
            }
        }
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: epoll_ctl: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }

    return 0;
}


int lnetwork_nats_connect_with_servinfo (
    struct lnetwork_s * lnetwork
)
{

    int ret = 0;

    if (NULL == lnetwork->nats.servinfo_p) {
        syslog(LOG_ERR, "%s:%d:%s: could not connect to " CONFIG_NATS_HOST ":"  CONFIG_NATS_PORT,
                __FILE__, __LINE__, __func__);
        return -1;
    }

    lnetwork->nats.fd = socket(
        lnetwork->nats.servinfo_p->ai_family,
        lnetwork->nats.servinfo_p->ai_socktype | SOCK_NONBLOCK,
        lnetwork->nats.servinfo_p->ai_protocol
    );
    if (-1 == lnetwork->nats.fd) {
        syslog(LOG_ERR, "%s:%d:%s: socket: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }

    ret = connect(
        lnetwork->nats.fd,
        lnetwork->nats.servinfo_p->ai_addr,
        lnetwork->nats.servinfo_p->ai_addrlen
    );
    if (-1 == ret && errno != EINPROGRESS) {
        syslog(LOG_WARNING, "%s:%d:%s: connect: %s", __FILE__, __LINE__, __func__, strerror(errno));
        close(lnetwork->nats.fd);
        lnetwork->nats.fd = 0;
        lnetwork->nats.servinfo_p = lnetwork->nats.servinfo_p->ai_next;
        return lnetwork_nats_connect_with_servinfo(lnetwork);
    }

    if (-1 == ret && errno == EINPROGRESS) {

        // Connecting asynchronously, add the fd to epoll and wait for an answer...
        ret = epoll_ctl(
            lnetwork->epollfd,
            EPOLL_CTL_ADD,
            lnetwork->nats.fd,
            &(struct epoll_event){
                .events = EPOLLIN | EPOLLONESHOT,
                .data = {
                    .fd = lnetwork->nats.fd
                }
            }
        );
        if (-1 == ret) {
            syslog(LOG_ERR, "%s:%d:%s: epoll_ctl: %s", __FILE__, __LINE__, __func__, strerror(errno));
            return -1;
        }

        // Also kick the watchdog in case we get a syn connect timeout or something...
        // arm timerfd
        ret = timerfd_settime(
            /* fd        = */ lnetwork->nats.watchdogfd,
            /* opt       = */ 0,
            /* timerspec = */ &(struct itimerspec) {
                .it_interval = {0},
                .it_value = {
                    .tv_sec  = CONFIG_NATS_CONNECT_TIMEOUT_S,
                    .tv_nsec = 0
                }
            },
            /* old_ts    = */ NULL
        );
        if (-1 == ret) {
            syslog(LOG_ERR, "%s:%d:%s: timerfd_settime: %s", __FILE__, __LINE__, __func__, strerror(errno));
            return -1;
        }

        // All done, wait for something to happen...
        return 0;
    }

    // If we reach this point, we connected non-asynchronously, in this case
    // we're just done. Clean up the structures and add it to epoll to start
    // handling it.
    freeaddrinfo(lnetwork->nats.servinfo);
    lnetwork->nats.servinfo = NULL;
    lnetwork->nats.servinfo_p = NULL;

    ret = epoll_ctl(
        lnetwork->epollfd,
        EPOLL_CTL_ADD,
        lnetwork->nats.fd,
        &(struct epoll_event){
            .events = EPOLLIN | EPOLLONESHOT,
            .data = {
                .fd = lnetwork->nats.fd
            }
        }
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: epoll_ctl: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }


    // start listening for netlink events
    ret = lnetwork_netlink_start(lnetwork);
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: lnetwork_netlink_start returned -1", __FILE__, __LINE__, __func__);
        return -1;
    }

    return 0;
}


int lnetwork_epoll_event_nats_connecting (
    struct lnetwork_s * lnetwork,
    struct epoll_event * event
)
{

    int ret = 0;
    int sockerr = 0;

    // Did we manage to connect?
    ret = getsockopt(
        /* fd = */ event->data.fd,
        /* level = */ SOL_SOCKET,
        /* option = */ SO_ERROR,
        /* ret = */ &sockerr,
        /* ret_len = */ &(socklen_t){sizeof(sockerr)}
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: getsockopt: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }

    // no, try the next result from getaddrinfo
    if (sockerr < 0) {
        syslog(LOG_WARNING, "%s:%d:%s: connect: %s", __FILE__, __LINE__, __func__, strerror(errno));
        lnetwork->nats.servinfo_p = lnetwork->nats.servinfo_p->ai_next;
        return lnetwork_nats_connect_with_servinfo(lnetwork);
    }

    // we've successfully connected, free the structures.
    freeaddrinfo(lnetwork->nats.servinfo);
    lnetwork->nats.servinfo = NULL;
    lnetwork->nats.servinfo_p = NULL;


    // re-arm it on epoll, wait for data...
    ret = epoll_ctl(
        lnetwork->epollfd,
        EPOLL_CTL_MOD,
        event->data.fd,
        &(struct epoll_event){
            .events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLONESHOT,
            .data = event->data
        }
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: epoll_ctl: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }


    // start listening for netlink events
    ret = lnetwork_netlink_start(lnetwork);
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: lnetwork_netlink_start returned -1", __FILE__, __LINE__, __func__);
        return -1;
    }

    return 0;
}


int lnetwork_epoll_event_nats_connected (
    struct lnetwork_s * lnetwork,
    struct epoll_event * event
)
{

    int ret = 0;
    int bytes_read = 0;
    uint8_t buf[2048];

    bytes_read = read(event->data.fd, buf, sizeof(buf));
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: read: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }
    if (0 == bytes_read) {
        syslog(LOG_ERR, "%s:%d:%s: nats server closed connection", __FILE__, __LINE__, __func__);
        return -1;
    }

    // ok we read some data - kick the watchdog
    // arm timerfd
    ret = timerfd_settime(
        /* fd        = */ lnetwork->nats.watchdogfd,
        /* opt       = */ 0,
        /* timerspec = */ &(struct itimerspec) {
            .it_interval = {0},
            .it_value = {
                .tv_sec  = CONFIG_NATS_TIMEOUT_S,
                .tv_nsec = 0
            }
        },
        /* old_ts    = */ NULL
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: timerfd_settime: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }
    
    // parse the received data
    ret = lnetwork_nats_parser_parse(&lnetwork->nats.parser, buf, bytes_read);
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: lnetwork_nats_parser_parse returned -1", __FILE__, __LINE__, __func__);
        return -1;
    }

    // re-arm the fd on epoll
    ret = epoll_ctl(
        lnetwork->epollfd,
        EPOLL_CTL_MOD,
        event->data.fd,
        &(struct epoll_event){
            .events = EPOLLIN | EPOLLONESHOT,
            .data = event->data
        }
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: epoll_ctl: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }

    return 0;
}


int lnetwork_epoll_event_nats (
    struct lnetwork_s * lnetwork,
    struct epoll_event * event
)
{
    // If we haven't zeroed out this yet, we're still connecting.
    if (NULL != lnetwork->nats.servinfo)
        return lnetwork_epoll_event_nats_connecting(lnetwork, event);

    else
        return lnetwork_epoll_event_nats_connected(lnetwork, event);
}


int lnetwork_epoll_event_nats_watchdog (
    struct lnetwork_s * lnetwork,
    struct epoll_event * event
)
{
    syslog(LOG_ERR, "%s:%d:%s: watchdog triggered", __FILE__, __LINE__, __func__);
    return -1;
}


static int lnetwork_epoll_event_dispatch (
    struct lnetwork_s * lnetwork,
    struct epoll_event * event
)
{
    if (lnetwork->netlinkfd == event->data.fd)
        return lnetwork_epoll_event_netlink(lnetwork, event);

    if (lnetwork->nats.fd == event->data.fd)
        return lnetwork_epoll_event_nats(lnetwork, event);

    if (lnetwork->nats.watchdogfd == event->data.fd)
        return lnetwork_epoll_event_nats_watchdog(lnetwork, event);

    syslog(LOG_ERR, "%s:%d:%s: No match on epoll event.", __FILE__, __LINE__, __func__);
    return -1;
}


static int lnetwork_epoll_handle_events (
    struct lnetwork_s * lnetwork,
    struct epoll_event epoll_events[EPOLL_NUM_EVENTS],
    int ep_events_len
)
{
    int ret = 0;
    for (int i = 0; i < ep_events_len; i++) {
        ret = lnetwork_epoll_event_dispatch(lnetwork, &epoll_events[i]);
        if (0 != ret) {
            return ret;
        }
    }
    return 0;
}


int lnetwork_loop (
    struct lnetwork_s * lnetwork
)
{

    int ret = 0;

    if (LNETWORK_SENTINEL != lnetwork->sentinel) {
        syslog(LOG_ERR, "%s:%d:%s: sentinel is wrong", __FILE__, __LINE__, __func__);
        return -1;
    }

    int ep_events_len = 0;
    struct epoll_event ep_events[EPOLL_NUM_EVENTS];
    while (1) {
        ep_events_len = epoll_wait(lnetwork->epollfd, ep_events, EPOLL_NUM_EVENTS, -1);
        if (-1 == ret && EINTR == errno) {
            continue;
        }
        if (-1 == ep_events_len) {
            syslog(LOG_ERR, "%s:%d:%s: epoll_wait returned -1", __FILE__, __LINE__, __func__);
            return -1;
        }

        ret = lnetwork_epoll_handle_events(lnetwork, ep_events, ep_events_len);
        if (-1 == ret) {
            syslog(LOG_ERR, "%s:%d:%s: lnetwork_epoll_handle_events returned -1", __FILE__, __LINE__, __func__);
            return -1;
        }
    }

    return 0;
}


int lnetwork_init (
    struct lnetwork_s * lnetwork
)
{
    int ret = 0;

    // Create the epoll instance
    lnetwork->epollfd = epoll_create1(EPOLL_CLOEXEC);
    if (-1 == lnetwork->epollfd) {
        syslog(LOG_ERR, "%s:%d:%s: epoll_create1: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }

    // And the watchdog
    lnetwork->nats.watchdogfd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
    if (-1 == lnetwork->nats.watchdogfd) {
        syslog(LOG_ERR, "%s:%d:%s: timerfd_create: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }

    // Add the watchdog to epoll
    ret = epoll_ctl(
        lnetwork->epollfd,
        EPOLL_CTL_ADD,
        lnetwork->nats.watchdogfd,
        &(struct epoll_event){
            .events = EPOLLIN | EPOLLONESHOT,
            .data = {
                .fd = lnetwork->nats.watchdogfd
            }
        }
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: epoll_ctl: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }

    lnetwork->sentinel = LNETWORK_SENTINEL;

    return 0;
}


int lnetwork_nats_event_info (
    void * user_data
)
{

    int ret = 0;
    uint8_t topic[256];
    int topic_len = 0;

    struct lnetwork_s * lnetwork = user_data;
    if (LNETWORK_SENTINEL != lnetwork->sentinel) {
        syslog(LOG_ERR, "%s:%d:%s: sentinel is wrong", __FILE__, __LINE__, __func__);
        return -1;
    }

    topic_len = snprintf((char*)topic, sizeof(topic), "host.%.*s.lnetwork.request", lnetwork->hostname_len, lnetwork->hostname);
    if (-1 == topic_len) {
        syslog(LOG_ERR, "%s:%d:%s: snprintf returned -1", __FILE__, __LINE__, __func__);
        return -1;
    }

    ret = sub(
        /* lnetwork = */ lnetwork,
        /* topic = */ (const char *)topic,
        /* topic_len = */ topic_len,
        /* sid = */ 1
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: sub returned -1", __FILE__, __LINE__, __func__);
        return -1;
    }

    return 0;
}


int lnetwork_nats_event_ping (
    void * user_data
)
{

    int bytes_written = 0;

    struct lnetwork_s * lnetwork = user_data;
    if (LNETWORK_SENTINEL != lnetwork->sentinel) {
        syslog(LOG_ERR, "%s:%d:%s: sentinel is wrong", __FILE__, __LINE__, __func__);
        return -1;
    }

    bytes_written = write(lnetwork->nats.fd, "PONG\r\n", 6);
    if (-1 == bytes_written) {
        syslog(LOG_ERR, "%s:%d:%s: write: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }
    if (6 != bytes_written) {
        syslog(LOG_ERR, "%s:%d:%s: partial write, wrote %d bytes, expected to write 6 bytes", __FILE__, __LINE__, __func__, bytes_written);
        return -1;
    }

    return 0;
}


int lnetwork_nats_event_request (
    void * user_data,
    const uint8_t * rt_topic,
    uint32_t rt_topic_len
)
{
    int ret = 0;
    int bytes_written = 0;
    unsigned char * db_p = NULL;
    sqlite3_int64 db_size = 0;

    struct lnetwork_s * lnetwork = user_data;
    if (LNETWORK_SENTINEL != lnetwork->sentinel) {
        syslog(LOG_ERR, "%s:%d:%s: sentinel is wrong", __FILE__, __LINE__, __func__);
        return -1;
    }

    syslog(LOG_INFO, "%s:%d:%s: got request", __FILE__, __LINE__, __func__);

    db_p = sqlite3_serialize(
        /* sqlite3 = */ lnetwork->db,
        /* db = */ "main",
        /* size = */ &db_size,
        /* flags = */ 0
    );
    if (NULL == db_p) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_serialize returned NULL: %s", __FILE__, __LINE__, __func__, sqlite3_errmsg(lnetwork->db));
        return -1;
    }
    if (db_size <= 0) {
        syslog(LOG_ERR, "%s:%d:%s: db_size=%lld", __FILE__, __LINE__, __func__, db_size);
    }

    ret = pub(
        /* lnetwork = */ lnetwork,
        /* topic = */ rt_topic,
        /* topic_len = */ rt_topic_len,
        /* rt = */ NULL,
        /* rt_len = */ 0,
        /* payload = */ db_p,
        /* payload_len = */ (uint32_t)db_size
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: pub returned -1", __FILE__, __LINE__, __func__);
        return -1;
    }

    return 0;
}


int lnetwork_nats_connect (
    struct lnetwork_s * lnetwork
)
{

    int ret = 0;

    // initialize the parser
    ret = lnetwork_nats_parser_init(
        /* parser = */ &lnetwork->nats.parser,
        /* info_cb = */ lnetwork_nats_event_info,
        /* ping_cb = */ lnetwork_nats_event_ping,
        /* lnetwork_request_cb = */ lnetwork_nats_event_request,
        /* user_data = */ lnetwork
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: lnetwork_nats_parser_init returned -1", __FILE__, __LINE__, __func__);
        return -1;
    }

    // getaddrinfo
    ret = getaddrinfo(
        /* host = */ CONFIG_NATS_HOST,
        /* port = */ CONFIG_NATS_PORT,
        /* hints = */ &(struct addrinfo) {
            .ai_family = AF_UNSPEC,
            .ai_socktype = SOCK_STREAM,
            .ai_flags = AI_PASSIVE
        },
        /* servinfo = */ &lnetwork->nats.servinfo
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: getaddrinfo: %s", __FILE__, __LINE__, __func__, gai_strerror(ret));
        return -1;
    }
    if (NULL == lnetwork->nats.servinfo) {
        syslog(LOG_ERR, "%s:%d:%s: no results from getaddrinfo", __FILE__, __LINE__, __func__);
        return -1;
    }

    lnetwork->nats.servinfo_p = lnetwork->nats.servinfo;

    return lnetwork_nats_connect_with_servinfo(lnetwork);
}


int lnetwork_hostname_init (
    struct lnetwork_s * lnetwork
)
{

    int ret = 0;
    char hostname[64];
    char safe_hostname[64];

    ret = gethostname(hostname, sizeof(hostname));
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: gethostname: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }

    ret = lnetwork_hostname_parser_parse((const uint8_t*)hostname, sizeof(hostname), lnetwork->hostname);
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: lnetwork_hostname_parser_parse returned -1", __FILE__, __LINE__, __func__);
        return -1;
    }

    lnetwork->hostname_len = ret;

    return 0;
}


int lnetwork_init_sqlite (
    struct lnetwork_s * lnetwork
)
{

    int ret = 0;
    char * err = NULL;

    // open sqlite database 
    ret = sqlite3_open_v2(
        /* path = */ "lnetwork.sqlite",
        /* db = */ &lnetwork->db,
        /* flags = */ SQLITE_OPEN_READWRITE | SQLITE_OPEN_MEMORY,
        /* vfs = */ NULL
    );
    if (SQLITE_OK != ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_open_v2 returned %d", __FILE__, __LINE__, __func__, ret);
        return -1;
    }

    ret = sqlite3_exec(
        /* db = */ lnetwork->db,
        /* sqlite = */ sqlite_schema,
        /* cb = */ NULL,
        /* user_data = */ NULL,
        /* err = */ &err
    );
    if (SQLITE_OK != ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_exec returned %d: %s", __FILE__, __LINE__, __func__, ret, err);
        return -1;
    }

    return 0;
}


int main (
    int argc,
    char const* argv[]
)
{
    int ret = 0;
    struct lnetwork_s lnetwork = {0};

    openlog("lnetwork", LOG_CONS | LOG_PID, LOG_USER);

    ret = lnetwork_init(&lnetwork);
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: lnetwork_init returned -1", __FILE__, __LINE__, __func__);
        return -1;
    }

    ret = lnetwork_init_sqlite(&lnetwork);
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: lnetwork_init_sqlite returned -1", __FILE__, __LINE__, __func__);
        return -1;
    }

    ret = lnetwork_hostname_init(&lnetwork);
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: lnetwork_hostname_init returned -1", __FILE__, __LINE__, __func__);
        return -1;
    }

    ret = lnetwork_nats_connect(&lnetwork);
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: lnetwork_start_nats returned -1", __FILE__, __LINE__, __func__);
        return -1;
    }

    ret = lnetwork_loop(&lnetwork);
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: lnetwork_loop returned -1", __FILE__, __LINE__, __func__);
        return -1;
    }

    return 0;
    (void)argc;
    (void)argv;
}
