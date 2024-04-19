#pragma once

#include <stdint.h>


uint8_t get_id(void);
struct rte_mempool *get_pktbuf_mempool(void);
struct rte_mempool *get_message_pool(void);
int32_t init(int32_t argc, char *argv[]);
