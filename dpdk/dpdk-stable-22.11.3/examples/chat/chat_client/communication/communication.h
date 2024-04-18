#pragma once

#include <stdint.h>

/* Number of packets to attempt to read from queue */
#define PKT_READ_SIZE  ((uint16_t)32)

#define MBQ_CAPACITY 32

struct rte_ring *get_rx_ring(void);
int32_t init_comm_port(void);
int32_t init_communication(void);
int32_t request_connection(void);
int32_t disconnect(void);
int32_t send_msg(char *__msg, int32_t __dst);
void send_data(void *buf, uint16_t size, uint16_t out_port);
void receive_data(void);
