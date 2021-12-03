#define _DEFAULT_SOURCE

#include <syslog.h>

#include "lnetwork_hostname_parser.h"


%%{
    machine hostname;

    action hostname_copy {
        hostname[hostname_len++] = *p;
    }

    action safe_copy {
        hostname[hostname_len++] = '-';
    }

    allowed = [A-Za-z0-9];
    others = (any - allowed - 0);

    main := (
        allowed $hostname_copy |
        others $safe_copy | 
        0 @{ return hostname_len; }
    ){1,64};
}%%


int lnetwork_hostname_parser_parse (
    const uint8_t * const buf,
    const uint32_t buf_len,
    char hostname[64]
)
{
    int ret = 0;
    int cs = 0;
    const char * p  = (const char*)buf;
    const char * pe = (const char*)buf + buf_len;
    const char * eof = 0;
    int hostname_len = 0;

    %% write data;
    %% write init;
    %% write exec;

    if (%%{ write error; }%% == cs) {
        // parser error
        return -1;
    }

    return hostname_len;
}
