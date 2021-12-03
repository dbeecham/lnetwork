#define _DEFAULT_SOURCE

#include <syslog.h>

#include "lnetwork_nats_parser.h"


%%{
    machine nats;
    access parser->;

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

    loop := (
        ping |
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
    void * user_data
)
{
    %% write init;
    parser->user_data = user_data;
    parser->info_cb = info_cb;
    parser->ping_cb = ping_cb;
    return 0;
}


int lnetwork_nats_parser_parse (
    struct lnetwork_nats_parser_s * parser,
    const uint8_t * const buf,
    const uint32_t buf_len
)
{
    int ret = 0;
    const char * p  = (const char*)buf;
    const char * pe = (const char*)buf + buf_len;
    const char * eof = 0;

    %% write exec;

    if (%%{ write error; }%% == parser->cs) {
        // parser error
        return -1;
    }

    return 0;
}
