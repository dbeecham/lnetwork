#define _DEFAULT_SOURCE

#include <asm/types.h>
#include <sys/types.h>
#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/if.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <netlink/attr.h>
#include <netlink/msg.h>

#include "lnetwork.h"
#include "lnetwork_hostname_parser.h"
#include "lnetwork_nats_parser.h"


static int sqlite3_exec_print (
    void * user_data,
    int argc,
    char ** argv,
    char ** names
)
{
    printf("%s:%d:%s: hi!\n", __FILE__, __LINE__, __func__);

    for (int i = 0; i < argc; i++) {
        printf("%s:%d:%s: %s: %s\n", __FILE__, __LINE__, __func__, names[i], argv[i]);
    }
    puts("");

    return 0;
    (void)user_data;
}


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
    const uint8_t * topic,
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
    const uint8_t * topic,
    const uint32_t topic_len,
    const uint8_t * rt,
    const uint32_t rt_len,
    const uint8_t sid,
    const uint8_t * payload,
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


int lnetwork_netlink_query_link (
    struct lnetwork_s * lnetwork
)
{
    syslog(LOG_DEBUG, "%s:%d:%s: hi!", __FILE__, __LINE__, __func__);

    struct {
        struct nlmsghdr nlmsghdr;
        struct ifinfomsg ifi;
    } req;

    int ret = 0;
    int bytes_written = 0;

    bytes_written = write(
        /* fd = */ lnetwork->netlinkfd,
        /* nlmsghdr = */ &(struct { struct nlmsghdr nlmsghdr; struct ifinfomsg ifi; }) {
            .nlmsghdr = {
                .nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg)),
                .nlmsg_type = RTM_GETLINK,
                .nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP,
                .nlmsg_seq = 1
            },
            .ifi = {
                .ifi_family = AF_UNSPEC,
                .ifi_change = 0xFFFFFFFF
            }
        },
        /* nlmsghdr_len = */ NLMSG_LENGTH(sizeof(struct ifinfomsg))
    );
    if (-1 == bytes_written) {
        syslog(LOG_ERR, "%s:%d:%s: write: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }

    lnetwork->queried_link = true;

    return 0;
}


int lnetwork_netlink_query_addr (
    struct lnetwork_s * lnetwork
)
{
    syslog(LOG_DEBUG, "%s:%d:%s: hi!", __FILE__, __LINE__, __func__);

    struct {
        struct nlmsghdr nlmsghdr;
        struct ifinfomsg ifi;
    } req;

    int ret = 0;
    int bytes_written = 0;

    bytes_written = write(
        /* fd = */ lnetwork->netlinkfd,
        /* nlmsghdr = */ &(struct { struct nlmsghdr nlmsghdr; struct ifinfomsg ifi; }) {
            .nlmsghdr = {
                .nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg)),
                .nlmsg_type = RTM_GETADDR,
                .nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP,
                .nlmsg_seq = 1
            },
            .ifi = {
                .ifi_family = AF_UNSPEC,
                .ifi_change = 0xFFFFFFFF
            }
        },
        /* nlmsghdr_len = */ NLMSG_LENGTH(sizeof(struct ifinfomsg))
    );
    if (-1 == bytes_written) {
        syslog(LOG_ERR, "%s:%d:%s: write: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }

    lnetwork->queried_addr = true;

    return 0;
}


int lnetwork_interface_add (
    struct lnetwork_s * lnetwork,
    unsigned int ifid
)
{
    int ret = 0;
    sqlite3_stmt * stmt;

    syslog(LOG_DEBUG, "%s:%d:%s: hi!", __FILE__, __LINE__, __func__);

    // prepare sqlite3 statement
    const char sql[] = "insert into interfaces(ifid) values (?) on conflict do nothing returning ifid;";
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
        /* int = */ ifid
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_bind_int returned %d: %s", __FILE__, __LINE__, __func__, ret, sqlite3_errmsg(lnetwork->db));
        return -1;
    }
     
    ret = sqlite3_step(stmt);
    if (SQLITE_DONE == ret) {
        // we're done - this is not a new ip address, no need to notify anyone.
        syslog(LOG_DEBUG, "%s:%d:%s: it's already in the database", __FILE__, __LINE__, __func__);
        sqlite3_finalize(stmt);
        return 0;
    }
    if (SQLITE_ROW != ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_step returned %d", __FILE__, __LINE__, __func__, ret);
        sqlite3_finalize(stmt);
        return -1;
        // this is a new address, let's notify people.
    }

    syslog(LOG_INFO, "%s:%d:%s: ok added!", __FILE__, __LINE__, __func__);
    sqlite3_finalize(stmt);
    return 1;
}


int lnetwork_interface_add_name (
    struct lnetwork_s * lnetwork,
    unsigned int ifid,
    const char * ifname,
    int ifname_len
)
{
    int ret = 0;
    sqlite3_stmt * stmt;

    syslog(LOG_DEBUG, "%s:%d:%s: hi!", __FILE__, __LINE__, __func__);

    // prepare sqlite3 statement
    const char sql[] = "insert into names(ifid, ifname) values (?,?) on conflict do nothing returning ifid;";
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
        /* int = */ ifid
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_bind_int returned %d: %s", __FILE__, __LINE__, __func__, ret, sqlite3_errmsg(lnetwork->db));
        return -1;
    }

    ret = sqlite3_bind_text(
        /* stmt = */ stmt,
        /* index = */ 2,
        /* text = */ ifname,
        /* text_len = */ strlen(ifname),
        /* mem_cb = */ SQLITE_STATIC
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_bind_text returned %d: %s", __FILE__, __LINE__, __func__, ret, sqlite3_errmsg(lnetwork->db));
        return -1;
    }
     
    ret = sqlite3_step(stmt);
    if (SQLITE_DONE == ret) {
        // we're done - this is not a new name, no need to notify anyone.
        syslog(LOG_DEBUG, "%s:%d:%s: it's already in the database", __FILE__, __LINE__, __func__);
        sqlite3_finalize(stmt);
        return 0;
    }
    if (SQLITE_ROW != ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_step returned %d", __FILE__, __LINE__, __func__, ret);
        sqlite3_finalize(stmt);
        return -1;
    }

    syslog(LOG_INFO, "%s:%d:%s: ok added!", __FILE__, __LINE__, __func__);
    sqlite3_finalize(stmt);
    return 1;
}


int lnetwork_interface_add_hwaddr (
    struct lnetwork_s * lnetwork,
    unsigned int ifid,
    const uint8_t * hwaddr,
    int hwaddr_len
)
{
    int ret = 0;
    sqlite3_stmt * stmt;

    syslog(LOG_DEBUG, "%s:%d:%s: hi! hwaddr_len=%d", __FILE__, __LINE__, __func__, hwaddr_len);

    // prepare sqlite3 statement
    const char sql[] = "insert into hwaddr(ifid, hwaddr) values (?,?) on conflict do update set hwaddr=excluded.hwaddr returning ifid;";
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
        /* int = */ ifid
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_bind_int returned %d: %s", __FILE__, __LINE__, __func__, ret, sqlite3_errmsg(lnetwork->db));
        return -1;
    }

    ret = sqlite3_bind_text(
        /* stmt = */ stmt,
        /* index = */ 2,
        /* text = */ hwaddr,
        /* text_len = */ hwaddr_len,
        /* mem_cb = */ SQLITE_STATIC
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_bind_text returned %d: %s", __FILE__, __LINE__, __func__, ret, sqlite3_errmsg(lnetwork->db));
        return -1;
    }
     
    ret = sqlite3_step(stmt);
    if (SQLITE_DONE == ret) {
        // we're done - this is not a new name, no need to notify anyone.
        syslog(LOG_DEBUG, "%s:%d:%s: no change", __FILE__, __LINE__, __func__);
        sqlite3_finalize(stmt);
        return 0;
    }
    if (SQLITE_ROW != ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_step returned %d: %s", __FILE__, __LINE__, __func__, ret, sqlite3_errmsg(lnetwork->db));
        sqlite3_finalize(stmt);
        return -1;
    }

    syslog(LOG_INFO, "%s:%d:%s: ok added!", __FILE__, __LINE__, __func__);
    sqlite3_finalize(stmt);
    return 1;
}


int lnetwork_interface_add_ipv6 (
    struct lnetwork_s * lnetwork,
    unsigned int ifid,
    const char * ipv6addr,
    int ipv6addr_len
)
{
    int ret = 0;
    sqlite3_stmt * stmt;

    // prepare sqlite3 statement
    const char sql[] = "insert into ipv6(ifid, ipv6addr) values (?,?) on conflict do nothing returning ifid;";
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
        /* int = */ ifid
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_bind_int returned %d: %s", __FILE__, __LINE__, __func__, ret, sqlite3_errmsg(lnetwork->db));
        return -1;
    }

    ret = sqlite3_bind_text(
        /* stmt = */ stmt,
        /* index = */ 2,
        /* text = */ ipv6addr,
        /* text_len = */ ipv6addr_len,
        /* mem_cb = */ SQLITE_STATIC
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_bind_text returned %d: %s", __FILE__, __LINE__, __func__, ret, sqlite3_errmsg(lnetwork->db));
        return -1;
    }
     
    ret = sqlite3_step(stmt);
    if (SQLITE_DONE == ret) {
        // we're done - this is not a new name, no need to notify anyone.
        syslog(LOG_DEBUG, "%s:%d:%s: it's already in the database", __FILE__, __LINE__, __func__);
        sqlite3_finalize(stmt);
        return 0;
    }
    if (SQLITE_ROW != ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_step returned %d", __FILE__, __LINE__, __func__, ret);
        sqlite3_finalize(stmt);
        return -1;
    }

    syslog(LOG_INFO, "%s:%d:%s: ok added!", __FILE__, __LINE__, __func__);
    sqlite3_finalize(stmt);
    return 1;
}


int lnetwork_interface_remove_ipv6 (
    struct lnetwork_s * lnetwork,
    unsigned int ifid,
    const char * ipv6addr,
    int ipv6addr_len
)
{
    int ret = 0;
    sqlite3_stmt * stmt;

    syslog(LOG_DEBUG, "%s:%d:%s: hi!", __FILE__, __LINE__, __func__);

    // prepare sqlite3 statement
    const char sql[] = "delete from ipv6 where ifid=? and ipv6addr=? returning ifid;";
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
        /* int = */ ifid
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_bind_int returned %d: %s", __FILE__, __LINE__, __func__, ret, sqlite3_errmsg(lnetwork->db));
        return -1;
    }

    // bind ipv6addr
    ret = sqlite3_bind_text(
        /* stmt = */ stmt,
        /* index = */ 2,
        /* text = */ ipv6addr,
        /* text_len = */ ipv6addr_len,
        /* mem_cb = */ SQLITE_STATIC
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_bind_text returned %d: %s", __FILE__, __LINE__, __func__, ret, sqlite3_errmsg(lnetwork->db));
        return -1;
    }
     
    ret = sqlite3_step(stmt);
    if (SQLITE_DONE == ret) {
        // we're done - this is not a new name, no need to notify anyone.
        syslog(LOG_DEBUG, "%s:%d:%s: it wasnt in the database, no notify needed", __FILE__, __LINE__, __func__);
        sqlite3_finalize(stmt);
        return 0;
    }
    if (SQLITE_ROW != ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_step returned %d", __FILE__, __LINE__, __func__, ret);
        sqlite3_finalize(stmt);
        return -1;
    }

    syslog(LOG_INFO, "%s:%d:%s: ok added!", __FILE__, __LINE__, __func__);
    sqlite3_finalize(stmt);
    return 1;
}


int lnetwork_interface_add_ipv4 (
    struct lnetwork_s * lnetwork,
    unsigned int ifid,
    const char * ipv4addr,
    int ipv4addr_len
)
{
    int ret = 0;
    sqlite3_stmt * stmt;

    syslog(LOG_DEBUG, "%s:%d:%s: hi!", __FILE__, __LINE__, __func__);

    // prepare sqlite3 statement
    const char sql[] = "insert into ipv4(ifid, ipv4addr) values (?,?) on conflict do nothing returning ifid;";
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
        /* int = */ ifid
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_bind_int returned %d: %s", __FILE__, __LINE__, __func__, ret, sqlite3_errmsg(lnetwork->db));
        return -1;
    }

    ret = sqlite3_bind_text(
        /* stmt = */ stmt,
        /* index = */ 2,
        /* text = */ ipv4addr,
        /* text_len = */ ipv4addr_len,
        /* mem_cb = */ SQLITE_STATIC
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_bind_text returned %d: %s", __FILE__, __LINE__, __func__, ret, sqlite3_errmsg(lnetwork->db));
        return -1;
    }
     
    ret = sqlite3_step(stmt);
    if (SQLITE_DONE == ret) {
        // we're done - this is not a new name, no need to notify anyone.
        syslog(LOG_DEBUG, "%s:%d:%s: it's already in the database", __FILE__, __LINE__, __func__);
        sqlite3_finalize(stmt);
        return 0;
    }
    if (SQLITE_ROW != ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_step returned %d", __FILE__, __LINE__, __func__, ret);
        sqlite3_finalize(stmt);
        return -1;
    }

    syslog(LOG_INFO, "%s:%d:%s: ok added!", __FILE__, __LINE__, __func__);
    sqlite3_finalize(stmt);
    return 1;
}


int lnetwork_interface_remove_ipv4 (
    struct lnetwork_s * lnetwork,
    unsigned int ifid,
    const char * ipv4addr,
    int ipv4addr_len
)
{
    int ret = 0;
    sqlite3_stmt * stmt;

    syslog(LOG_DEBUG, "%s:%d:%s: hi!", __FILE__, __LINE__, __func__);

    // prepare sqlite3 statement
    const char sql[] = "delete from ipv4 where ifid=? and ipv4addr=? returning ifid;";
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
        /* int = */ ifid
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_bind_int returned %d: %s", __FILE__, __LINE__, __func__, ret, sqlite3_errmsg(lnetwork->db));
        return -1;
    }

    // bind ipv4addr
    ret = sqlite3_bind_text(
        /* stmt = */ stmt,
        /* index = */ 2,
        /* text = */ ipv4addr,
        /* text_len = */ ipv4addr_len,
        /* mem_cb = */ SQLITE_STATIC
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_bind_text returned %d: %s", __FILE__, __LINE__, __func__, ret, sqlite3_errmsg(lnetwork->db));
        return -1;
    }
     
    ret = sqlite3_step(stmt);
    if (SQLITE_DONE == ret) {
        // we're done - this is not a new name, no need to notify anyone.
        syslog(LOG_DEBUG, "%s:%d:%s: it wasnt in the database, no notify needed", __FILE__, __LINE__, __func__);
        sqlite3_finalize(stmt);
        return 0;
    }
    if (SQLITE_ROW != ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_step returned %d", __FILE__, __LINE__, __func__, ret);
        sqlite3_finalize(stmt);
        return -1;
    }

    syslog(LOG_INFO, "%s:%d:%s: ok added!", __FILE__, __LINE__, __func__);
    sqlite3_finalize(stmt);
    return 1;
}


int lnetwork_interface_add_mtu (
    struct lnetwork_s * lnetwork,
    unsigned int ifid,
    int mtu
)
{
    int ret = 0;
    sqlite3_stmt * stmt;

    syslog(LOG_DEBUG, "%s:%d:%s: hi!", __FILE__, __LINE__, __func__);

    // prepare sqlite3 statement
#warning TODO: this needs a returning clause, but also a check for value change
    const char sql[] = "insert into mtu(ifid, mtu) values (?,?) on conflict do update set mtu=excluded.mtu;";
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
        /* int = */ ifid
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_bind_int returned %d: %s", __FILE__, __LINE__, __func__, ret, sqlite3_errmsg(lnetwork->db));
        return -1;
    }

    ret = sqlite3_bind_int(
        /* stmt = */ stmt,
        /* index = */ 2,
        /* int = */ mtu
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_bind_int returned %d: %s", __FILE__, __LINE__, __func__, ret, sqlite3_errmsg(lnetwork->db));
        return -1;
    }
     
    ret = sqlite3_step(stmt);
    if (SQLITE_DONE == ret) {
        // we're done - this is not a new name, no need to notify anyone.
        syslog(LOG_DEBUG, "%s:%d:%s: it's already in the database", __FILE__, __LINE__, __func__);
        sqlite3_finalize(stmt);
        return 0;
    }
    if (SQLITE_ROW != ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_step returned %d", __FILE__, __LINE__, __func__, ret);
        sqlite3_finalize(stmt);
        return -1;
    }

    syslog(LOG_INFO, "%s:%d:%s: ok added!", __FILE__, __LINE__, __func__);
    sqlite3_finalize(stmt);
    return 1;
}


int lnetwork_interface_add_txqlen (
    struct lnetwork_s * lnetwork,
    unsigned int ifid,
    int txqlen
)
{
    int ret = 0;
    sqlite3_stmt * stmt;

    syslog(LOG_DEBUG, "%s:%d:%s: hi!", __FILE__, __LINE__, __func__);

    // prepare sqlite3 statement
#warning TODO: this needs a returning clause, but also a check for value change
    const char sql[] = "insert into txqlen(ifid, txqlen) values (?,?) on conflict do update set txqlen=excluded.txqlen;";
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
        /* int = */ ifid
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_bind_int returned %d: %s", __FILE__, __LINE__, __func__, ret, sqlite3_errmsg(lnetwork->db));
        return -1;
    }

    ret = sqlite3_bind_int(
        /* stmt = */ stmt,
        /* index = */ 2,
        /* int = */ txqlen
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_bind_int returned %d: %s", __FILE__, __LINE__, __func__, ret, sqlite3_errmsg(lnetwork->db));
        return -1;
    }
     
    ret = sqlite3_step(stmt);
    if (SQLITE_DONE == ret) {
        // we're done - this is not a new name, no need to notify anyone.
        syslog(LOG_DEBUG, "%s:%d:%s: no change", __FILE__, __LINE__, __func__);
        sqlite3_finalize(stmt);
        return 0;
    }
    if (SQLITE_ROW != ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_step returned %d", __FILE__, __LINE__, __func__, ret);
        sqlite3_finalize(stmt);
        return -1;
    }

    syslog(LOG_INFO, "%s:%d:%s: txqlen added", __FILE__, __LINE__, __func__);
    sqlite3_finalize(stmt);
    return 1;
}


int lnetwork_interface_add_promisuity (
    struct lnetwork_s * lnetwork,
    unsigned int ifid
)
{
    int ret = 0;
    sqlite3_stmt * stmt;

    syslog(LOG_DEBUG, "%s:%d:%s: hi!", __FILE__, __LINE__, __func__);

    // prepare sqlite3 statement
#warning TODO: this needs a returning clause, but also a check for value change
    const char sql[] = "insert into promisuous(ifid) values (?) on conflict do nothing returning ifid;";
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
        /* int = */ ifid
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_bind_int returned %d: %s", __FILE__, __LINE__, __func__, ret, sqlite3_errmsg(lnetwork->db));
        return -1;
    }

    ret = sqlite3_step(stmt);
    if (SQLITE_DONE == ret) {
        // we're done - this is not a new name, no need to notify anyone.
        syslog(LOG_DEBUG, "%s:%d:%s: it's already in the database", __FILE__, __LINE__, __func__);
        sqlite3_finalize(stmt);
        return 0;
    }
    if (SQLITE_ROW != ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_step returned %d", __FILE__, __LINE__, __func__, ret);
        sqlite3_finalize(stmt);
        return -1;
    }

    syslog(LOG_INFO, "%s:%d:%s: ok added!", __FILE__, __LINE__, __func__);
    sqlite3_finalize(stmt);
    return 1;
}


int lnetwork_interface_remove_promisuity (
    struct lnetwork_s * lnetwork,
    unsigned int ifid
)
{
    int ret = 0;
    sqlite3_stmt * stmt;

    syslog(LOG_DEBUG, "%s:%d:%s: hi!", __FILE__, __LINE__, __func__);

    // prepare sqlite3 statement
#warning TODO: this needs a returning clause, but also a check for value change
    const char sql[] = "delete from promisuous where ifid=? returning ifid;";
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
        /* int = */ ifid
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_bind_int returned %d: %s", __FILE__, __LINE__, __func__, ret, sqlite3_errmsg(lnetwork->db));
        return -1;
    }

    ret = sqlite3_step(stmt);
    if (SQLITE_DONE == ret) {
        // we're done - this is not a new name, no need to notify anyone.
        syslog(LOG_DEBUG, "%s:%d:%s: no change", __FILE__, __LINE__, __func__);
        sqlite3_finalize(stmt);
        return 0;
    }
    if (SQLITE_ROW != ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_step returned %d", __FILE__, __LINE__, __func__, ret);
        sqlite3_finalize(stmt);
        return -1;
    }

    syslog(LOG_INFO, "%s:%d:%s: changed", __FILE__, __LINE__, __func__);
    sqlite3_finalize(stmt);
    return 1;
}


int lnetwork_interface_remove (
    struct lnetwork_s * lnetwork,
    unsigned int ifid
)
{

    int ret = 0;
    sqlite3_stmt * stmt;

    syslog(LOG_DEBUG, "%s:%d:%s: hi!", __FILE__, __LINE__, __func__);

    // prepare sqlite3 statement
    const char sql[] = "delete from interfaces where ifid=? returning ifid;";
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
        /* int = */ ifid
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_bind_int returned %d: %s", __FILE__, __LINE__, __func__, ret, sqlite3_errmsg(lnetwork->db));
        return -1;
    }
     
    ret = sqlite3_step(stmt);
    if (SQLITE_DONE == ret) {
        // we're done - this is not a new ip address, no need to notify anyone.
        syslog(LOG_DEBUG, "%s:%d:%s: it wasnt in the database", __FILE__, __LINE__, __func__);
        sqlite3_finalize(stmt);
        return 0;
    }
    if (SQLITE_ROW != ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_step returned %d", __FILE__, __LINE__, __func__, ret);
        sqlite3_finalize(stmt);
        return -1;
        // this is a new address, let's notify people.
    }

    syslog(LOG_INFO, "%s:%d:%s: ok deleted", __FILE__, __LINE__, __func__);
    sqlite3_finalize(stmt);
    return 0;
}


int lnetwork_notify_interface (
    struct lnetwork_s * lnetwork,
    int ifid
)
{
    int ret = 0;

    syslog(LOG_INFO, "%s:%d:%s: notify interface %d", __FILE__, __LINE__, __func__, ifid);

    return 0;
}


int lnetwork_epoll_event_netlink_newlink (
    struct lnetwork_s * lnetwork,
    struct epoll_event * event,
    struct ifinfomsg * ifi,
    uint32_t ifi_len,
    struct nlattr * nlattr,
    uint32_t nlattr_len
)
{
    int ret = 0;
    char name[IFNAMSIZ];
    bool notify_needed = false;

    // this is called when an interface gets link info, but its also called
    // when link is removed from an interface. You need to check the flags to
    // see what state it's in. It's also called when a new interface is added.


    syslog(LOG_INFO, "%s:%d:%s: interface %d just got link, type=%d, flags=%d, family=%d, running=%d",
            __FILE__, __LINE__, __func__, ifi->ifi_index, ifi->ifi_type, ifi->ifi_flags, ifi->ifi_family, ifi->ifi_flags & IFF_RUNNING);


    ret = lnetwork_interface_add(lnetwork, ifi->ifi_index);
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: lnetwork_interface_add returned -1", __FILE__, __LINE__, __func__);
        return -1;
    }
    if (1 == ret) {
        notify_needed = true;
    }


    // See linux/include/linux/socket.h:176 for list of definitions
    if (AF_UNSPEC == ifi->ifi_family) {
        syslog(LOG_INFO, "%s:%d:%s: AF_UNSPEC", __FILE__, __LINE__, __func__);
    }
    else if (AF_UNIX == ifi->ifi_family) {
        syslog(LOG_INFO, "%s:%d:%s: AF_UNIX", __FILE__, __LINE__, __func__);
    }
    else if (AF_INET == ifi->ifi_family) {
        syslog(LOG_INFO, "%s:%d:%s: AF_INET", __FILE__, __LINE__, __func__);
    }
    else if (AF_BRIDGE == ifi->ifi_family) {
        syslog(LOG_INFO, "%s:%d:%s: AF_BRIDGE", __FILE__, __LINE__, __func__);
    }
    else {
        syslog(LOG_INFO, "%s:%d:%s: unknown interface family", __FILE__, __LINE__, __func__);
    }

    struct nlattr * attr = nlattr;
    int attr_len = nlattr_len;
    for (; nla_ok(attr, attr_len); attr = nla_next(attr, &attr_len)) {
        if (IFLA_UNSPEC == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: unspec", __FILE__, __LINE__, __func__);
            continue;
        }
        if (IFLA_ADDRESS == nla_type(attr)) {
            char buffer[64];
            const uint8_t * addr = nla_data(attr);
            int buffer_len = snprintf(buffer, sizeof(buffer), "%02x:%02x:%02x:%02x:%02x:%02x", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
            ret = lnetwork_interface_add_hwaddr(lnetwork, ifi->ifi_index, buffer, buffer_len);
            if (-1 == ret) {
                syslog(LOG_ERR, "%s:%d:%s: lnetwork_interface_add_hwaddr returned -1", __FILE__, __LINE__, __func__);
                return -1;
            }
            continue;
        }
        if (IFLA_BROADCAST == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: broadcast", __FILE__, __LINE__, __func__);
            continue;
        }
        if (IFLA_IFNAME == nla_type(attr)) {
            ret = lnetwork_interface_add_name(lnetwork, ifi->ifi_index, nla_data(attr), nla_len(attr));
            if (-1 == ret) {
                syslog(LOG_ERR, "%s:%d:%s: lnetwork_interface_add_name returned -1", __FILE__, __LINE__, __func__);
                return -1;
            }
            continue;
        }
        if (IFLA_MTU == nla_type(attr)) {
            int * mtu = nla_data(attr);
            syslog(LOG_INFO, "%s:%d:%s: mtu: %d", __FILE__, __LINE__, __func__, *mtu);
            ret = lnetwork_interface_add_mtu(lnetwork, ifi->ifi_index, *mtu);
            if (-1 == ret) {
                syslog(LOG_ERR, "%s:%d:%s: lnetwork_interface_add_mtu returned -1", __FILE__, __LINE__, __func__);
                return -1;
            }
            continue;
        }
        if (IFLA_LINK == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: link", __FILE__, __LINE__, __func__);
            continue;
        }
        if (IFLA_QDISC == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: qdisc", __FILE__, __LINE__, __func__);
            continue;
        }
        if (IFLA_STATS == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: stats", __FILE__, __LINE__, __func__);
            continue;
        }
        if (IFLA_STATS64 == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: stats64", __FILE__, __LINE__, __func__);
            continue;
        }
        if (IFLA_AF_SPEC == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: AF_SPEC", __FILE__, __LINE__, __func__);
            continue;
        }
        if (IFLA_VF_PORTS == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: IFLA_VF_PORTS", __FILE__, __LINE__, __func__);
            continue;
        }
        if (IFLA_GROUP == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: group", __FILE__, __LINE__, __func__);
            continue;
        }
        if (IFLA_PROMISCUITY == nla_type(attr)) {
            uint32_t * promiscuity = nla_data(attr);
            if (0 == *promiscuity) {
                ret = lnetwork_interface_remove_promisuity(lnetwork, ifi->ifi_index);
                if (-1 == ret) {
                    syslog(LOG_ERR, "%s:%d:%s: lnetwork_interface_remove_promisuity returned -1", __FILE__, __LINE__, __func__);
                    return -1;
                }
            }
            else {
                ret = lnetwork_interface_add_promisuity(lnetwork, ifi->ifi_index);
                if (-1 == ret) {
                    syslog(LOG_ERR, "%s:%d:%s: lnetwork_interface_add_promisuity returned -1", __FILE__, __LINE__, __func__);
                    return -1;
                }
            }
            continue;
        }
        if (IFLA_TXQLEN == nla_type(attr)) {
            uint32_t * txqlen = nla_data(attr);
            ret = lnetwork_interface_add_txqlen(lnetwork, ifi->ifi_index, *txqlen);
            if (-1 == ret) {
                syslog(LOG_ERR, "%s:%d:%s: lnetwork_interface_add_txqlen returned -1", __FILE__, __LINE__, __func__);
                return -1;
            }
            continue;
        }
        if (IFLA_WIRELESS == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: wireless", __FILE__, __LINE__, __func__);
            continue;
        }
        if (IFLA_MAP == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: map", __FILE__, __LINE__, __func__);
            continue;
        }
        if (IFLA_WEIGHT == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: weight", __FILE__, __LINE__, __func__);
            continue;
        }
        if (IFLA_OPERSTATE == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: IFLA_OPERSTATE", __FILE__, __LINE__, __func__);
            int * operstate = nla_data(attr);
            if (IF_OPER_UNKNOWN == *operstate) {
                syslog(LOG_INFO, "%s:%d:%s: IFLA_OPERSTATE=IF_OPER_UNKNOWN", __FILE__, __LINE__, __func__);
            }
            if (IF_OPER_DOWN == *operstate) {
                syslog(LOG_INFO, "%s:%d:%s: IFLA_OPERSTATE=IF_OPER_DOWN", __FILE__, __LINE__, __func__);
            }
            if (IF_OPER_LOWERLAYERDOWN == *operstate) {
                syslog(LOG_INFO, "%s:%d:%s: IFLA_OPERSTATE=IF_OPER_LOWERLAYERDOWN", __FILE__, __LINE__, __func__);
            }
            if (IF_OPER_TESTING == *operstate) {
                syslog(LOG_INFO, "%s:%d:%s: IFLA_OPERSTATE=IF_OPER_TESTING", __FILE__, __LINE__, __func__);
            }
            if (IF_OPER_DORMANT == *operstate) {
                syslog(LOG_INFO, "%s:%d:%s: IFLA_OPERSTATE=IF_OPER_DORMANT", __FILE__, __LINE__, __func__);
            }
            if (IF_OPER_UP == *operstate) {
                syslog(LOG_INFO, "%s:%d:%s: IFLA_OPERSTATE=IF_OPER_UP", __FILE__, __LINE__, __func__);
            }
            continue;
        }
        if (IFLA_LINKMODE == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: IFLA_LINKMODE", __FILE__, __LINE__, __func__);
            continue;
        }
        if (IFLA_LINKINFO == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: IFLA_LINKINFO, len=%d", __FILE__, __LINE__, __func__, nla_len(attr));
            continue;
        }
        if (IFLA_VLAN_ID == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: IFLA_VLAN_ID", __FILE__, __LINE__, __func__);
            continue;
        }
        if (IFLA_NET_NS_PID == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: IFLA_NET_NS_PID, len=%d", __FILE__, __LINE__, __func__, nla_len(attr));
            continue;
        }
        if (IFLA_NUM_TX_QUEUES == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: IFLA_NUM_TX_QUEUES", __FILE__, __LINE__, __func__);
            continue;
        }
        if (IFLA_NUM_RX_QUEUES == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: IFLA_NUM_RX_QUEUES", __FILE__, __LINE__, __func__);
            continue;
        }
        if (IFLA_CARRIER == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: IFLA_CARRIER", __FILE__, __LINE__, __func__);
            continue;
        }
        if (IFLA_CARRIER_CHANGES == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: IFLA_CARRIER_CHANGES", __FILE__, __LINE__, __func__);
            continue;
        }
        if (IFLA_CARRIER_UP_COUNT == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: IFLA_CARRIER_UP_COUNT", __FILE__, __LINE__, __func__);
            continue;
        }
        if (IFLA_IF_NETNSID == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: IFLA_IF_NETNSID", __FILE__, __LINE__, __func__);
            continue;
        }
        if (IFLA_EVENT == nla_type(attr)) {
            // See linux/net/rtnetlink.c:rtnl_get_event and linux/net/core/rtnetlink.c:rtnetlink_event
            uint32_t * event = nla_data(attr);

            if (IFLA_EVENT_REBOOT == *event) {
                syslog(LOG_INFO, "%s:%d:%s: IFLA_EVENT=IFLA_EVENT_REBOOT", __FILE__, __LINE__, __func__);
                continue;
            }
            if (IFLA_EVENT_FEATURES == *event) {
                syslog(LOG_INFO, "%s:%d:%s: IFLA_EVENT=IFLA_EVENT_FEATURES", __FILE__, __LINE__, __func__);
                continue;
            }
            if (IFLA_EVENT_BONDING_FAILOVER == *event) {
                syslog(LOG_INFO, "%s:%d:%s: IFLA_EVENT=IFLA_EVENT_BONDING_FAILOVER", __FILE__, __LINE__, __func__);
                continue;
            }
            if (IFLA_EVENT_NOTIFY_PEERS == *event) {
                syslog(LOG_INFO, "%s:%d:%s: IFLA_EVENT=IFLA_EVENT_NOTIFY_PEERS", __FILE__, __LINE__, __func__);
                continue;
            }
            if (IFLA_EVENT_IGMP_RESEND == *event) {
                syslog(LOG_INFO, "%s:%d:%s: IFLA_EVENT=IFLA_EVENT_IGMP_RESEND", __FILE__, __LINE__, __func__);
                continue;
            }
            if (IFLA_EVENT_BONDING_OPTIONS == *event) {
                syslog(LOG_INFO, "%s:%d:%s: IFLA_EVENT=IFLA_EVENT_BONDING_OPTIONS", __FILE__, __LINE__, __func__);
                continue;
            }

            syslog(LOG_INFO, "%s:%d:%s: unknown IFLA_EVENT", __FILE__, __LINE__, __func__);
            continue;
        }
        if (IFLA_PROTO_DOWN == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: IFLA_PROTO_DOWN", __FILE__, __LINE__, __func__);
            continue;
        }
        if (IFLA_NEW_NETNSID == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: IFLA_NEW_NETNSID", __FILE__, __LINE__, __func__);
            continue;
        }
        if (IFLA_CARRIER_DOWN_COUNT == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: IFLA_CARRIER_DOWN_COUNT", __FILE__, __LINE__, __func__);
            continue;
        }
        if (IFLA_PHYS_PORT_ID == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: IFLA_PHYS_PORT_ID", __FILE__, __LINE__, __func__);
            continue;
        }
        if (IFLA_NEW_IFINDEX == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: IFLA_NEW_IFINDEX", __FILE__, __LINE__, __func__);
            continue;
        }
        if (IFLA_PAD == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: IFLA_PAD", __FILE__, __LINE__, __func__);
            continue;
        }
        if (IFLA_XDP == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: IFLA_XDP", __FILE__, __LINE__, __func__);
            continue;
        }
        if (IFLA_GSO_MAX_SEGS == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: IFLA_GSO_MAX_SEGS", __FILE__, __LINE__, __func__);
            continue;
        }
        if (IFLA_GSO_MAX_SIZE == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: IFLA_GSO_MAX_SIZE", __FILE__, __LINE__, __func__);
            continue;
        }
        if (IFLA_MIN_MTU == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: IFLA_MIN_MTU", __FILE__, __LINE__, __func__);
            continue;
        }
        if (IFLA_MAX_MTU == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: IFLA_MAX_MTU", __FILE__, __LINE__, __func__);
            continue;
        }
        if (IFLA_PORT_SELF == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: IFLA_PORT_SELF", __FILE__, __LINE__, __func__);
            continue;
        }
        syslog(LOG_INFO, "%s:%d:%s: unknown type=%d", __FILE__, __LINE__, __func__, nla_type(attr));
    }



    if (IFF_UP == (IFF_UP & ifi->ifi_flags)) {
        syslog(LOG_INFO, "%s:%d:%s: its up", __FILE__, __LINE__, __func__);
    }

    if (IFF_LOWER_UP == (IFF_LOWER_UP & ifi->ifi_flags)) {
        syslog(LOG_INFO, "%s:%d:%s: IFF_LOWER_UP", __FILE__, __LINE__, __func__);
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
        syslog(LOG_INFO, "%s:%d:%s: promiscuous mode", __FILE__, __LINE__, __func__);
    }

    if (IFF_ALLMULTI == (IFF_ALLMULTI & ifi->ifi_flags)) {
        syslog(LOG_INFO, "%s:%d:%s: allmulti", __FILE__, __LINE__, __func__);
    }

    if (IFF_MULTICAST == (IFF_MULTICAST & ifi->ifi_flags)) {
        syslog(LOG_INFO, "%s:%d:%s: multicast", __FILE__, __LINE__, __func__);
    }

    if (IFF_NOTRAILERS == (IFF_NOTRAILERS & ifi->ifi_flags)) {
        syslog(LOG_INFO, "%s:%d:%s: IFF_NOTRAILERS", __FILE__, __LINE__, __func__);
    }

    if (IFF_MASTER == (IFF_MASTER & ifi->ifi_flags)) {
        syslog(LOG_INFO, "%s:%d:%s: IFF_MASTER", __FILE__, __LINE__, __func__);
    }

    if (IFF_SLAVE == (IFF_SLAVE & ifi->ifi_flags)) {
        syslog(LOG_INFO, "%s:%d:%s: IFF_SLAVE", __FILE__, __LINE__, __func__);
    }

    if (IFF_PORTSEL == (IFF_PORTSEL & ifi->ifi_flags)) {
        syslog(LOG_INFO, "%s:%d:%s: IFF_PORTSEL", __FILE__, __LINE__, __func__);
    }

    if (IFF_AUTOMEDIA == (IFF_AUTOMEDIA & ifi->ifi_flags)) {
        syslog(LOG_INFO, "%s:%d:%s: IFF_AUTOMEDIA", __FILE__, __LINE__, __func__);
    }

    if (IFF_DYNAMIC == (IFF_DYNAMIC & ifi->ifi_flags)) {
        syslog(LOG_INFO, "%s:%d:%s: IFF_DYNAMIC", __FILE__, __LINE__, __func__);
    }

    if (IFF_DORMANT == (IFF_DORMANT & ifi->ifi_flags)) {
        syslog(LOG_INFO, "%s:%d:%s: IFF_DORMANT", __FILE__, __LINE__, __func__);
    }

    if (IFF_ECHO == (IFF_ECHO & ifi->ifi_flags)) {
        syslog(LOG_INFO, "%s:%d:%s: IFF_ECHO", __FILE__, __LINE__, __func__);
    }


    if (true == notify_needed) {
        ret = lnetwork_notify_interface(lnetwork, ifi->ifi_index);
        if (-1 == ret) {
            syslog(LOG_ERR, "%s:%d:%s: lnetwork_notify_interface returned -1", __FILE__, __LINE__, __func__);
            return -1;
        }
    }

    return 0;
}


int lnetwork_epoll_event_netlink_dellink (
    struct lnetwork_s * lnetwork,
    struct epoll_event * event,
    struct ifinfomsg * ifi,
    uint32_t ifi_len,
    struct nlattr * nlattr,
    uint32_t nlattr_len
)
{
    // This is called when link is removed from a device; this probably means
    // the device was removed entirely from the device (e.g. unplugged a USB
    // ethernet device, or called 'ip link del veth').

    char name[IFNAMSIZ];
    int ret = 0;
    bool notify_needed = false;

    ret = lnetwork_interface_remove(lnetwork, ifi->ifi_index);
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: lnetwork_interface_remove returned -1", __FILE__, __LINE__, __func__);
        return -1;
    }
    if (1 == ret) {
        notify_needed = true;
        syslog(LOG_INFO, "%s:%d:%s: it was removed", __FILE__, __LINE__, __func__);
        return 0;
    }

    return 0;

    syslog(LOG_INFO, "%s:%d:%s: interface %d just got removed, type=%d, flags=%d, family=%d",
            __FILE__, __LINE__, __func__, ifi->ifi_index, ifi->ifi_type, ifi->ifi_flags, ifi->ifi_family);

    if (IFLA_UNSPEC == (IFLA_UNSPEC & ifi->ifi_type)) {
        syslog(LOG_INFO, "%s:%d:%s: IFLA_UNSPEC", __FILE__, __LINE__, __func__);
    }

    if (IFLA_ADDRESS == (IFLA_ADDRESS & ifi->ifi_type)) {
        syslog(LOG_INFO, "%s:%d:%s: IFLA_ADDRESS", __FILE__, __LINE__, __func__);
    }

    if (IFLA_BROADCAST == (IFLA_BROADCAST & ifi->ifi_type)) {
        syslog(LOG_INFO, "%s:%d:%s: IFLA_BROADCAST", __FILE__, __LINE__, __func__);
    }

    if (IFLA_IFNAME == (IFLA_IFNAME & ifi->ifi_type)) {
        syslog(LOG_INFO, "%s:%d:%s: IFLA_IFNAME", __FILE__, __LINE__, __func__);
    }

    if (IFLA_MTU == (IFLA_MTU & ifi->ifi_type)) {
        syslog(LOG_INFO, "%s:%d:%s: IFLA_MTU", __FILE__, __LINE__, __func__);
    }

    if (IFLA_LINK == (IFLA_LINK & ifi->ifi_type)) {
        syslog(LOG_INFO, "%s:%d:%s: IFLA_LINK", __FILE__, __LINE__, __func__);
    }

    if (IFLA_QDISC == (IFLA_QDISC & ifi->ifi_type)) {
        syslog(LOG_INFO, "%s:%d:%s: IFLA_QDISC", __FILE__, __LINE__, __func__);
    }

    if (IFLA_STATS == (IFLA_STATS & ifi->ifi_type)) {
        syslog(LOG_INFO, "%s:%d:%s: IFLA_STATS", __FILE__, __LINE__, __func__);
    }

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


    if (true == notify_needed) {
        ret = lnetwork_notify_interface(lnetwork, ifi->ifi_index);
        if (-1 == ret) {
            syslog(LOG_ERR, "%s:%d:%s: lnetwork_notify_interface returned -1", __FILE__, __LINE__, __func__);
            return -1;
        }
    }

    return 0;
}


int lnetwork_epoll_event_netlink_deladdr_ipv6 (
    struct lnetwork_s * lnetwork,
    struct epoll_event * event,
    const struct nlmsghdr * const nlmsghdr,
    const struct ifaddrmsg * const ifa,
    struct in6_addr * in6_addr,
    int in6_addr_len
)
{
    bool notify_needed = false;
    int ret = 0;
    char addr[INET6_ADDRSTRLEN];
    char name[IFNAMSIZ] = {0};

    // get ip as a printable string
    if (NULL == inet_ntop(AF_INET6, in6_addr, addr, INET6_ADDRSTRLEN)) {
        syslog(LOG_ERR, "%s:%d:%s: inet_ntop: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }

    syslog(LOG_INFO, "%s:%d:%s: interface %d (%s) just lost addr %s",
            __FILE__, __LINE__, __func__, ifa->ifa_index, name, addr);

    ret = lnetwork_interface_remove_ipv6(lnetwork, ifa->ifa_index, addr, INET6_ADDRSTRLEN);
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: lnetwork_interface_remove_ipv6 returned -1", __FILE__, __LINE__, __func__);
        return -1;
    }
    if (1 == ret) {
        notify_needed = true;
    }

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


    if (true == notify_needed) {
        ret = lnetwork_notify_interface(lnetwork, ifa->ifa_index);
        if (-1 == ret) {
            syslog(LOG_ERR, "%s:%d:%s: lnetwork_notify_interface returned -1", __FILE__, __LINE__, __func__);
            return -1;
        }
    }

    return 0;
}


int lnetwork_epoll_event_netlink_newaddr_ipv6 (
    struct lnetwork_s * lnetwork,
    struct epoll_event * event,
    const struct nlmsghdr * const nlmsghdr,
    const struct ifaddrmsg * const ifa,
    uint32_t ifa_len,
    struct in6_addr * in6_addr,
    int in6_addr_len
)
{
    int ret = 0;
    char addr[INET6_ADDRSTRLEN];
    char ifname[IFNAMSIZ] = {0};
    char payload[4096];
    int payload_len = 0;
    uint8_t topic[512];
    int topic_len = 0;
    int bytes_written = 0;
    sqlite3_stmt * stmt;


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
    const char sql[] = "insert into ipv6(ifid, ipv6addr) values (?,?) on conflict(ifid,ipv6addr) do nothing returning ifid";
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
//    ret = sqlite3_bind_text(
//        /* stmt = */ stmt,
//        /* index = */ 2,
//        /* text = */ ifname,
//        /* text_len = */ sizeof(ifname),
//        /* mem_cb = */ SQLITE_STATIC
//    );
//    if (-1 == ret) {
//        syslog(LOG_ERR, "%s:%d:%s: sqlite3_bind_text returned %d: %s", __FILE__, __LINE__, __func__, ret, sqlite3_errmsg(lnetwork->db));
//        return -1;
//    }

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
        syslog(LOG_INFO, "%s:%d:%s: its an old ipv6 addr", __FILE__, __LINE__, __func__);
        sqlite3_finalize(stmt);
        return 0;
    }
    if (SQLITE_ROW == ret) {
        syslog(LOG_INFO, "%s:%d:%s: it's a new ipv6 addr", __FILE__, __LINE__, __func__);
        // this is a new address, let's notify people.
    }


    topic_len = snprintf((char*)topic, sizeof(topic), "lnetwork.%.*s.interface.%s.out", lnetwork->hostname_len, lnetwork->hostname, ifname);
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
        /* payload = */ (uint8_t*)payload,
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
    int ifa_len,
    struct in_addr * in_addr,
    int in_addr_len
)
{
    int ret = 0;
    char addr[INET_ADDRSTRLEN];
    char name[IFNAMSIZ] = {0};

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
    struct in_addr * in_addr,
    int in_addr_len
)
{
    // this function can also be called when an interface is removed, so we
    // can't be sure that we can call if_indextoname just yet.

    char addr[INET_ADDRSTRLEN];
    char name[IFNAMSIZ];

    if (NULL == inet_ntop(AF_INET, in_addr, addr, INET_ADDRSTRLEN)) {
        syslog(LOG_ERR, "%s:%d:%s: inet_ntop: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }

    syslog(LOG_INFO, "%s:%d:%s: interface %d just lost addr %s",
            __FILE__, __LINE__, __func__, ifa->ifa_index, addr);

    return 0;
}


int lnetwork_epoll_event_netlink_newaddr (
    struct lnetwork_s * lnetwork,
    struct epoll_event * event,
    const struct nlmsghdr * const nlmsghdr,
    const struct ifaddrmsg * const ifa,
    uint32_t ifa_len,
    struct nlattr * nlattr,
    int nlattr_len
)
{
    int ret = 0;
    int i = 0;
    bool notify_needed = false;

    struct nlattr * attr = nlattr;
    int attr_len = nlattr_len;
    while (1) {

        if (IFA_LOCAL == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: local", __FILE__, __LINE__, __func__);
        }

        else if (IFA_ADDRESS == nla_type(attr) && AF_INET6 == ifa->ifa_family && IFA_F_TEMPORARY == (IFA_F_TEMPORARY & ifa->ifa_flags)) {
            char addr[INET6_ADDRSTRLEN];
            // get ip as a printable string
            if (NULL == inet_ntop(AF_INET6, nla_data(attr), addr, INET6_ADDRSTRLEN)) {
                syslog(LOG_ERR, "%s:%d:%s: inet_ntop: %s", __FILE__, __LINE__, __func__, strerror(errno));
                return -1;
            }
            syslog(LOG_INFO, "%s:%d:%s: temporary ipv6 address=%s", __FILE__, __LINE__, __func__, addr);
        }

        else if (IFA_ADDRESS == nla_type(attr) && AF_INET6 == ifa->ifa_family && IFA_F_TENTATIVE == (IFA_F_TENTATIVE & ifa->ifa_flags)) {
            char addr[INET6_ADDRSTRLEN];
            // get ip as a printable string
            if (NULL == inet_ntop(AF_INET6, nla_data(attr), addr, INET6_ADDRSTRLEN)) {
                syslog(LOG_ERR, "%s:%d:%s: inet_ntop: %s", __FILE__, __LINE__, __func__, strerror(errno));
                return -1;
            }
            syslog(LOG_INFO, "%s:%d:%s: tentative ipv6 address=%s", __FILE__, __LINE__, __func__, addr);
        }

        else if (IFA_ADDRESS == nla_type(attr) && AF_INET6 == ifa->ifa_family) {

            char addr[INET6_ADDRSTRLEN];
            // get ip as a printable string
            if (NULL == inet_ntop(AF_INET6, nla_data(attr), addr, INET6_ADDRSTRLEN)) {
                syslog(LOG_ERR, "%s:%d:%s: inet_ntop: %s", __FILE__, __LINE__, __func__, strerror(errno));
                return -1;
            }
            syslog(LOG_INFO, "%s:%d:%s: ipv6 address=%s", __FILE__, __LINE__, __func__, addr);

            ret = lnetwork_interface_add_ipv6(lnetwork, ifa->ifa_index, addr, INET6_ADDRSTRLEN);
            if (-1 == ret) {
                syslog(LOG_ERR, "%s:%d:%s: lnetwork_interface_add_ipv6 returned -1", __FILE__, __LINE__, __func__);
                return -1;
            }
            if (1 == ret) {
                notify_needed = true;
            }

        }

        else if (IFA_ADDRESS == nla_type(attr) && AF_INET == ifa->ifa_family) {
            char addr[INET_ADDRSTRLEN];
            if (NULL == inet_ntop(AF_INET, nla_data(attr), addr, INET_ADDRSTRLEN)) {
                syslog(LOG_ERR, "%s:%d:%s: inet_ntop: %s", __FILE__, __LINE__, __func__, strerror(errno));
                return -1;
            }

            syslog(LOG_INFO, "%s:%d:%s: interface=%d, addr=%s", __FILE__, __LINE__, __func__, ifa->ifa_index, addr);

            ret = lnetwork_interface_add_ipv4(lnetwork, ifa->ifa_index, addr, INET_ADDRSTRLEN);
            if (-1 == ret) {
                syslog(LOG_ERR, "%s:%d:%s: lnetwork_interface_add_ipv4 returned -1", __FILE__, __LINE__, __func__);
                return -1;
            }
            if (1 == ret) {
                notify_needed = true;
            }
        }

        else if (IFA_LABEL == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: label", __FILE__, __LINE__, __func__);
        }

        else if (IFA_BROADCAST == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: broadcast addr", __FILE__, __LINE__, __func__);
        }

        else if (IFA_ANYCAST == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: anycast addr", __FILE__, __LINE__, __func__);
        }


        // ok fetch the next rt attribute
        attr = nla_next(attr, &attr_len);
        if (!nla_ok(attr, attr_len)) {
            break;
        }

        if (1024 < ++i) {
            syslog(LOG_ERR, "%s:%d:%s: infinite loop", __FILE__, __LINE__, __func__);
            return -1;
        }
    }

    if (true == notify_needed) {
        ret = lnetwork_notify_interface(lnetwork, ifa->ifa_index);
        if (-1 == ret) {
            syslog(LOG_ERR, "%s:%d:%s: lnetwork_notify_interface returned -1", __FILE__, __LINE__, __func__);
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
    struct nlattr * nlattr,
    int nlattr_len
)
{
    int ret = 0;
    bool notify_needed = false;

    struct nlattr * attr = nlattr;
    int attr_len = nlattr_len;
    int i = 0;
    while (1) {

        if (IFA_LOCAL == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: local", __FILE__, __LINE__, __func__);
        }

        else if (IFA_ADDRESS == nla_type(attr) && AF_INET6 == ifa->ifa_family) {

            ret = lnetwork_epoll_event_netlink_deladdr_ipv6(
                /* lnetwork = */ lnetwork,
                /* epoll event = */ event,
                /* nlmsghdr = */ nlmsghdr,
                /* ifaddrmsg = */ ifa,
                /* in6_addr = */ nla_data(attr),
                /* in6_addr_len = */ nla_len(attr)
            );
            if (-1 == ret) {
                syslog(LOG_ERR, "%s:%d:%s: lnetwork_epoll_event_netlink_deladdr_ipv6 returned -1", __FILE__, __LINE__, __func__);
                return -1;
            }
            if (1 == ret) {
                notify_needed = true;
            }
        }

        else if (IFA_ADDRESS == nla_type(attr) && AF_INET == ifa->ifa_family) {
            ret = lnetwork_epoll_event_netlink_deladdr_ipv4(
                /* lnetwork = */ lnetwork,
                /* epoll event = */ event,
                /* nlmsghdr = */ nlmsghdr,
                /* ifaddrmsg = */ ifa,
                /* in_addr = */ nla_data(attr),
                /* in_addr_len = */ nla_len(attr)
            );
            if (-1 == ret) {
                syslog(LOG_ERR, "%s:%d:%s: lnetwork_epoll_event_netlink_deladdr_ipv4 returned -1", __FILE__, __LINE__, __func__);
                return -1;
            }
        }

        else if (IFA_LABEL == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: label", __FILE__, __LINE__, __func__);
        }

        else if (IFA_BROADCAST == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: broadcast addr", __FILE__, __LINE__, __func__);
        }

        else if (IFA_ANYCAST == nla_type(attr)) {
            syslog(LOG_INFO, "%s:%d:%s: anycast addr", __FILE__, __LINE__, __func__);
        }


        // ok fetch the next rt attribute
        attr = nla_next(attr, &attr_len);
        if (!nla_ok(attr, attr_len)) {
            break;
        }

        if (1024 < ++i) {
            syslog(LOG_ERR, "%s:%d:%s: infinite loop", __FILE__, __LINE__, __func__);
            return -1;
        }
    }

    if (true == notify_needed) {
        ret = lnetwork_notify_interface(lnetwork, ifa->ifa_index);
        if (-1 == ret) {
            syslog(LOG_ERR, "%s:%d:%s: lnetwork_notify_interface returned -1", __FILE__, __LINE__, __func__);
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

    syslog(LOG_DEBUG, "%s:%d:%s: hi!", __FILE__, __LINE__, __func__);

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
    if (!nlmsg_ok(nlmsghdr, bytes_read)) {
        syslog(LOG_ERR, "%s:%d:%s: NLMSG_OK(nlmsghdr) returned false", __FILE__, __LINE__, __func__);
        return -1;
    }


    int i = 0;
    while (1) {

        // dispatch on netlink message type
        switch (nlmsghdr->nlmsg_type) {

        case NLMSG_DONE: {
            if (true == lnetwork->queried_link && false == lnetwork->queried_addr) {
                ret = lnetwork_netlink_query_addr(lnetwork);
                if (-1 == ret) {
                    syslog(LOG_ERR, "%s:%d:%s: lnetwork_network_query_addr returned -1", __FILE__, __LINE__, __func__);
                    return -1;
                }
            }
            syslog(LOG_INFO, "%s:%d:%s: done", __FILE__, __LINE__, __func__);
            break;
        }

        case RTM_NEWLINK: {
            ret = lnetwork_epoll_event_netlink_newlink(
                lnetwork,
                event,
                nlmsg_data(nlmsghdr),
                nlmsg_datalen(nlmsghdr),
                nlmsg_attrdata(nlmsghdr, sizeof(struct ifinfomsg)),
                nlmsg_attrlen(nlmsghdr, sizeof(struct ifinfomsg))
            );
            if (-1 == ret) {
                syslog(LOG_ERR, "%s:%d:%s: lnetwork_epoll_event_netlink_newlink returned -1",
                        __FILE__, __LINE__, __func__);
                return -1;
            }
            break;
        }

        case RTM_DELLINK: {
            ret = lnetwork_epoll_event_netlink_dellink(
                lnetwork,
                event,
                (struct ifinfomsg*)nlmsg_data(nlmsghdr),
                nlmsg_datalen(nlmsghdr),
                nlmsg_attrdata(nlmsghdr, sizeof(struct ifinfomsg)),
                nlmsg_attrlen(nlmsghdr, sizeof(struct ifinfomsg))
            );
            if (-1 == ret) {
                syslog(LOG_ERR, "%s:%d:%s: lnetwork_epoll_event_netlink_dellink returned -1", __FILE__, __LINE__, __func__);
                return -1;
            }
            break;
        }

        case RTM_NEWADDR: {
            ret = lnetwork_epoll_event_netlink_newaddr(
                /* lnetwork = */ lnetwork,
                /* epoll event = */ event,
                /* nlmsghdr = */ nlmsghdr,
                /* ifaddrmsg = */ nlmsg_data(nlmsghdr),
                /* ifaddrmsg_len = */ nlmsg_datalen(nlmsghdr),
                /* nlattr = */ nlmsg_attrdata(nlmsghdr, sizeof(struct ifaddrmsg)),
                /* nlattr_len = */ nlmsg_attrlen(nlmsghdr, sizeof(struct ifaddrmsg))
            );
            if (-1 == ret) {
                syslog(LOG_ERR, "%s:%d:%s: lnetwork_epoll_event_netlink_newaddr returned -1", __FILE__, __LINE__, __func__);
                return -1;
            }
            break;
        }

        case RTM_DELADDR: {
            ret = lnetwork_epoll_event_netlink_deladdr(
                /* lnetwork = */ lnetwork,
                /* epoll event = */ event,
                /* nlmsghdr = */ nlmsghdr,
                /* ifaddrmsg = */ nlmsg_data(nlmsghdr),
                /* nlattr = */ nlmsg_attrdata(nlmsghdr, sizeof(struct ifaddrmsg)),
                /* nlattr_len = */ nlmsg_attrlen(nlmsghdr, sizeof(struct ifaddrmsg))
            );
            if (-1 == ret) {
                syslog(LOG_ERR, "%s:%d:%s: lnetwork_epoll_event_netlink_deladdr returned -1", __FILE__, __LINE__, __func__);
                return -1;
            }
            break;
        }

        case RTM_NEWROUTE: {
            syslog(LOG_INFO, "%s:%d:%s: RTM_NEWROUTE", __FILE__, __LINE__, __func__);
            break;
        }

        case RTM_DELROUTE: {
            syslog(LOG_INFO, "%s:%d:%s: RTM_DELROUTE", __FILE__, __LINE__, __func__);
            break;
        }

        case RTM_NEWNEIGH: {
            syslog(LOG_INFO, "%s:%d:%s: RTM_NEWNEIGH", __FILE__, __LINE__, __func__);
            break;
        }

        case RTM_DELNEIGH: {
            syslog(LOG_INFO, "%s:%d:%s: RTM_DELNEIGH", __FILE__, __LINE__, __func__);
            break;
        }

        default: {
            syslog(LOG_ERR, "%s:%d:%s: no match on netlink message type %d", __FILE__, __LINE__, __func__, nlmsghdr->nlmsg_type);
            return -1;
        }

        }


        // get the next nlmsghdr packet
        nlmsghdr = nlmsg_next(nlmsghdr, &bytes_read);
        if (!nlmsg_ok(nlmsghdr, bytes_read)) {
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

    ret = lnetwork_netlink_query_link(lnetwork);
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: lnetwork_netlink_query returned -1", __FILE__, __LINE__, __func__);
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
    int32_t topic_len;
    struct lnetwork_s * lnetwork = user_data;

    topic_len = snprintf((char*)topic, sizeof(topic), "lnetwork.%.*s.request", lnetwork->hostname_len, lnetwork->hostname);
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: snprintf returned -1", __FILE__, __LINE__, __func__);
        return -1;
    }

    ret = sub(
        /* lnetwork = */ lnetwork,
        /* topic = */ topic,
        /* topic_len = */ topic_len,
        /* sid = */ 1
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: sub returned -1", __FILE__, __LINE__, __func__);
        return -1;
    }

    if (LNETWORK_SENTINEL != lnetwork->sentinel) {
        syslog(LOG_ERR, "%s:%d:%s: sentinel is wrong", __FILE__, __LINE__, __func__);
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
    char * debug_err;

    struct lnetwork_s * lnetwork = user_data;
    if (LNETWORK_SENTINEL != lnetwork->sentinel) {
        syslog(LOG_ERR, "%s:%d:%s: sentinel is wrong", __FILE__, __LINE__, __func__);
        return -1;
    }

    syslog(LOG_INFO, "%s:%d:%s: got request", __FILE__, __LINE__, __func__);

    char * debug_sql =
        "select * from interfaces natural left join names natural left join ipv6 natural left join ipv4 natural left join mtu natural left join txqlen natural left join promisuous natural left join hwaddr;";
    ret = sqlite3_exec(
        /* db = */ lnetwork->db,
        /* sql = */ debug_sql,
        /* cb = */ sqlite3_exec_print,
        /* user_data = */ NULL,
        /* err = */ &debug_err
    );
    if (SQLITE_OK != ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_exec returned %d: %s",
            __FILE__, __LINE__, __func__, ret, debug_err);
        sqlite3_free(debug_err);
    }

    debug_sql =
        "select * from ipv4;";
    ret = sqlite3_exec(
        /* db = */ lnetwork->db,
        /* sql = */ debug_sql,
        /* cb = */ sqlite3_exec_print,
        /* user_data = */ NULL,
        /* err = */ &debug_err
    );
    if (SQLITE_OK != ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_exec returned %d: %s",
            __FILE__, __LINE__, __func__, ret, debug_err);
        sqlite3_free(debug_err);
    }

    debug_sql =
        "select * from names;";
    ret = sqlite3_exec(
        /* db = */ lnetwork->db,
        /* sql = */ debug_sql,
        /* cb = */ sqlite3_exec_print,
        /* user_data = */ NULL,
        /* err = */ &debug_err
    );
    if (SQLITE_OK != ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_exec returned %d: %s",
            __FILE__, __LINE__, __func__, ret, debug_err);
        sqlite3_free(debug_err);
    }

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
        /* request_cb = */ lnetwork_nats_event_request,
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
        /* sqlite = */ "pragma foreign_keys=1;",
        /* cb = */ NULL,
        /* user_data = */ NULL,
        /* err = */ &err
    );
    if (SQLITE_OK != ret) {
        syslog(LOG_ERR, "%s:%d:%s: sqlite3_exec returned %d: %s", __FILE__, __LINE__, __func__, ret, err);
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

    openlog("lnetwork", LOG_NDELAY | LOG_PERROR, LOG_USER);

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
