#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#include "capstone.h"

#define MAX_CLIENTS             16

#define MP_CLIENT_RXQ_NAME "MProc_Client_%u_RX"
#define PKTMBUF_POOL_NAME "MProc_pktmbuf_pool"
#define MZ_PORT_INFO "MProc_port_info"
#define CONN_RX_RING "MProcSrv_Conn_Rx_Ring"
#define CONN_TX_RING "MProcSrv_Conn_Tx_Ring"
#define COMM_RX_RING "MProcSrv_Comm_Rx_%u_Ring"
#define COMM_TX_RING "MProcSrv_Comm_Tx_%u_Ring"
#define MSG_POOL_NAME "MProc_msg_pool"

#define MSG_SIZE 2048
#define MSG_DTBUF_SIZE(msg_off) ((MSG_SIZE) - (msg_off))

struct port_info {
	uint16_t num_ports;
	uint16_t id[RTE_MAX_ETHPORTS];
    /**
     * INFO: Possible add this information at some point. There is no need for it not.
    */
	// volatile struct rx_stats rx_stats;
	// volatile struct tx_stats tx_stats[MAX_CLIENTS];
};

/*
 * Given the rx queue name template above, get the queue name
 */
static inline const char *
get_rx_queue_name(uint8_t id)
{
	/* buffer for return value. Size calculated by %u being replaced
	 * by maximum 3 digits (plus an extra byte for safety) */
	static char buffer[sizeof(MP_CLIENT_RXQ_NAME) + 2];

	snprintf(buffer, sizeof(buffer), MP_CLIENT_RXQ_NAME, id);
	return buffer;
}

static inline char *
get_ring_name(const char *fmt, uint8_t id)
{
	char *res;

	if (!asprintf(&res, fmt, id))
		return NULL;

	return res;
}

dom_id_t get_domain_id(void);
void set_domain_id(dom_id_t __id);
int get_dev_fd(void);
void set_dev_fd(int __fd);

enum MSG_TYPE {
	CONNECT,
	DISCONNECT
};

struct ipc_message {
	enum MSG_TYPE m_type;
	char m_dtbuf[MSG_DTBUF_SIZE(sizeof(enum MSG_TYPE))];
};

struct client_domain {
	dom_id_t id;
	region_id_t metadata_region_id;
	region_id_t send_region_id;
	region_id_t receive_region_id;
};

struct client_domain *get_client_domains(void);
void set_client_domains(struct client_domain *__client_domains);
