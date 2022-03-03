#pragma once

#include <stdint.h>

struct lnetwork_nats_parser_s {
    int cs;
    int (*info_cb)(void * user_data);
    int (*ping_cb)(void * user_data);
    int (*lnetwork_request_cb)(void * user_data, const uint8_t * rt_topic, uint32_t rt_topic_len);
    uint8_t rt_topic[128];
    uint32_t rt_topic_len;
    void * user_data;
};

int lnetwork_nats_parser_init (
    struct lnetwork_nats_parser_s * parser,
    int (*info_cb)(void * user_data),
    int (*ping_cb)(void * user_data),
    int (*lnetwork_request_cb)(void * user_data),
    void * user_data
);

int lnetwork_nats_parser_parse (
    struct lnetwork_nats_parser_s * parser,
    const uint8_t * const buf,
    const uint32_t buf_len
);
