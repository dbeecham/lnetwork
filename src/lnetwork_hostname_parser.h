#pragma once

#include <stdint.h>

int lnetwork_hostname_parser_parse (
    const uint8_t * const buf,
    const uint32_t buf_len,
    char hostname[64]
);
