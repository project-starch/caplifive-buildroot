#pragma once

#include <stdint.h>

#include <rte_common.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_byteorder.h>
#include <rte_launch.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_debug.h>
#include <rte_ring.h>
#include <rte_log.h>
#include <rte_mempool.h>
#include <rte_memcpy.h>
#include <rte_mbuf.h>
#include <rte_interrupts.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_string_fns.h>
#include <rte_cycles.h>

#define MBUF_CACHE_SIZE 512

#define RTE_MP_RX_DESC_DEFAULT 1024
#define RTE_MP_TX_DESC_DEFAULT 1024
#define CLIENT_QUEUE_RINGSIZE 128

#define NO_FLAGS 0

/*
 * Define a client structure with all needed info, including
 * stats from the clients.
 */
struct client {
	struct rte_ring *tx_q;
	struct rte_ring *rx_q;
	unsigned client_id;
	/* these stats hold how many packets the client will actually receive,
	 * and how many packets were dropped because the client's queue was full.
	 * The port-info stats, in contrast, record how many packets were received
	 * or transmitted on an actual NIC port.
	 */
	struct {
		volatile uint64_t rx;
		volatile uint64_t rx_drop;
	} stats;
};

struct cclient {
	struct rte_ring *m_tx_q;
	struct rte_ring *m_rx_q;
};

struct port_info *get_ports(void);
struct client *get_clients(void);
struct cclient *get_cclients(void);
struct rte_ring *get_conn_rx_ring(void);
struct rte_ring *get_conn_tx_ring(void);
struct rte_mempool *get_msg_pool(void);

int32_t init(int32_t argc, char *argv[]);
