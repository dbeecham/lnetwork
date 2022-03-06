#define _DEFAULT_SOURCE

#include <syslog.h>

#include "lnetwork_nats_parser.h"


%%{
    machine nats;
    access parser->;

    alphtype unsigned char;

    action ping {
        ret = parser->ping_cb(parser->user_data);
        if (-1 == ret) {
            syslog(LOG_ERR, "%s:%d:%s: parser->ping_cb returned -1", __FILE__, __LINE__, __func__);
            return -1;
        }
        fgoto loop;
    }

    ping = (
        'PING\r\n' @ping
    );

    ok = '+OK\r\n' @{ fgoto loop; };

    hostname = (any - '.')*;

    action init_rt_topic {
        parser->rt_topic_len = 0;
    }
    action copy_rt_topic {
        parser->rt_topic[parser->rt_topic_len++] = *p;
    }
    rt_topic = [0-9A-Za-z\._\-]{1,128} >to(init_rt_topic) $copy_rt_topic;

    action lnetwork_request {
        ret = parser->request_cb(parser->user_data, parser->rt_topic, parser->rt_topic_len);
        if (-1 == ret) {
            syslog(LOG_ERR, "%s:%d:%s: parser->lnetwork_request_cb returned -1", __FILE__, __LINE__, __func__);
            return -1;
        }
        fgoto loop;
    }
    lnetwork_request =
        'MSG lnetwork.'
        hostname
        '.request'
        ' '
        '1'
        ' '
        rt_topic
        ' '
        '0'
        '\r\n'
        '\r\n' @lnetwork_request;

    loop := (
        ping |
        lnetwork_request |
        ok
    ) $err{ 
        syslog(LOG_ERR, "%s:%d:%s: parse failed at 0x%02x (buf=%.*s)",
            __FILE__, __LINE__, __func__, *p, buf_len, buf
        ); 
        return -1; 
    };

    action connected {
        ret = parser->info_cb(parser->user_data);
        if (-1 == ret) {
            syslog(LOG_ERR, "%s:%d:%s: parser->info_cb returned -1", __FILE__, __LINE__, __func__);
            return -1;
        }

        fgoto loop;
    }

    # we always expect an info message first
    info = 
        'INFO'
        ' '*
        '{'
        (any - '}')*
        '}'
        ' '*
        '\r\n' @connected;

    main := info;

    write data;
}%%


int lnetwork_nats_parser_init (
    struct lnetwork_nats_parser_s * parser,
    int info_cb(void * user_data),
    int ping_cb(void * user_data),
    int (*request_cb)(void * user_data, const uint8_t * rt_topic, uint32_t rt_topic_len),
    void * user_data
)
{
    %% write init;
    parser->user_data = user_data;
    parser->info_cb = info_cb;
    parser->ping_cb = ping_cb;
    parser->request_cb = request_cb;
    return 0;
}


int lnetwork_nats_parser_parse (
    struct lnetwork_nats_parser_s * parser,
    const uint8_t * const buf,
    const uint32_t buf_len
)
{
    int ret = 0;
    const uint8_t * p  = buf;
    const uint8_t * pe = buf + buf_len;
    const uint8_t * eof = 0;

    %% write exec;

    if (%%{ write error; }%% == parser->cs) {
        // parser error
        return -1;
    }

    return 0;
}
