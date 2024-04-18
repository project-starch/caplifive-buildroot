#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/queue.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>

#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_branch_prediction.h>
#include <rte_log.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_ring.h>
#include <rte_launch.h>
#include <rte_debug.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_interrupts.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_string_fns.h>

#include "init.h"
#include "commons.h"
#include "communication.h"

static struct rte_ring *rx_ring;
static struct rte_eth_dev_tx_buffer *tx_buffer;

static struct rte_ring *conn_rx_ring = NULL;
static struct rte_ring *conn_tx_ring = NULL;

static struct rte_ring *comm_rx_ring = NULL;
static struct rte_ring *comm_tx_ring = NULL;

struct rte_ring *
get_rx_ring(void)
{
    return rx_ring;
}

int32_t
init_comm_port(void)
{
    tx_buffer = rte_zmalloc_socket("tx_buffer",
            RTE_ETH_TX_BUFFER_SIZE(MBQ_CAPACITY), 0, 1);
    if (tx_buffer == NULL)
        rte_exit(EXIT_FAILURE, "Cannot allocate buffer for tx.\n");

    rte_eth_tx_buffer_init(tx_buffer, MBQ_CAPACITY);

    rx_ring = rte_ring_lookup(get_rx_queue_name(1));
	if (rx_ring == NULL)
		rte_exit(EXIT_FAILURE, "Cannot get RX ring - is server process running?\n");

    return 0;
}

int32_t
init_communication(void)
{
    uint8_t client_id = get_id();

    tx_buffer = rte_zmalloc_socket("tx_buffer",
			RTE_ETH_TX_BUFFER_SIZE(MBQ_CAPACITY), 0,
			rte_eth_dev_socket_id(client_id));
    if (tx_buffer == NULL)
		rte_exit(EXIT_FAILURE, "Cannot allocate buffer for tx on port %u\n", client_id);

    rte_eth_tx_buffer_init(tx_buffer, MBQ_CAPACITY);

    rx_ring = rte_ring_lookup(get_rx_queue_name(client_id));
	if (rx_ring == NULL)
		rte_exit(EXIT_FAILURE, "Cannot get RX ring - is server process running?\n");

    conn_rx_ring = rte_ring_lookup(CONN_RX_RING);
    if (conn_rx_ring == NULL)
        rte_exit(EXIT_FAILURE, "Could not find connection ring. Is the server up?\n");

    conn_tx_ring = rte_ring_lookup(CONN_TX_RING);
    if (conn_tx_ring == NULL)
        rte_exit(EXIT_FAILURE,  "Could not find connection ring. Is the server up?\n");

    char *srx = get_ring_name(COMM_RX_RING, client_id);
    fprintf(stdout, "Initializing rx queue %s.\n", srx);
    comm_rx_ring = rte_ring_lookup(srx);
    if (comm_rx_ring == NULL)
        rte_exit(EXIT_FAILURE, "Could not find connection ring. Is the server up?\n");

    char *stx = get_ring_name(COMM_TX_RING, client_id);
    fprintf(stdout, "Initializing tx queue %s.\n", srx);
    comm_tx_ring = rte_ring_lookup(stx);
    if (comm_tx_ring == NULL)
        rte_exit(EXIT_FAILURE, "Could not find connection ring. Is the server up?\n");

    return 0;
}

int32_t
request_connection(void)
{
    struct rte_mempool *msg_pool = get_message_pool();
    struct ipc_message *conn_msg, *ack_msg = NULL;

    rte_mempool_get(msg_pool, (void **)&conn_msg);

    if (conn_msg == NULL) {
        fprintf(stderr, "Cannot obtain memory for connection message.\n");
        return -1;
    }

    conn_msg->m_type = CONNECT;
    sprintf(conn_msg->m_dtbuf, "%d", rte_lcore_id());

    if (rte_ring_enqueue(conn_rx_ring, conn_msg) < 0) {
        fprintf(stderr, "Could not send connection request.\n");
        return -1;
    }

    /**
     * Check if the ack is received
     */
    while (rte_ring_dequeue(conn_tx_ring, (void **)&ack_msg) < 0);
    if (ack_msg == NULL) {
        fprintf(stderr, "Could not receive connection ack.\n");
        return -1;
    }

    /**
     * Check if ack is correct
     */
    if ((conn_msg->m_type != ack_msg->m_type) || strcmp(conn_msg->m_dtbuf, ack_msg->m_dtbuf)) {
        fprintf(stderr, "Received bad ack.\n");
        return 1;
    }

    rte_mempool_put(msg_pool, (void *)conn_msg);

    fprintf(stdout, "Connected.\n");

    return 0;
}

int32_t
disconnect(void)
{
    struct rte_mempool *msg_pool = get_message_pool();
    struct ipc_message *conn_msg;
    struct ipc_message *ack_msg;

    rte_mempool_get(msg_pool, (void **)&conn_msg);

    if (conn_msg == NULL) {
        fprintf(stderr, "Cannot obtain memory for connection message.\n");
        return -1;
    }

    conn_msg->m_type = DISCONNECT;
    sprintf(conn_msg->m_dtbuf, "%d", rte_lcore_id());

    if (rte_ring_enqueue(conn_rx_ring, conn_msg) < 0) {
        fprintf(stderr, "Could not send disconnect request.\n");
        return -1;
    }

    /**
     * Check if the ack is received
     */
    while (rte_ring_dequeue(conn_tx_ring, (void **)&ack_msg) < 0);
    if (ack_msg == NULL) {
        fprintf(stderr, "Could not receive connection ack.\n");
        return -1;
    }

    /**
     * Check if ack is correct
     */
    if ((conn_msg->m_type != ack_msg->m_type) || strcmp(conn_msg->m_dtbuf, ack_msg->m_dtbuf)) {
        fprintf(stderr, "Received bad ack.\n");
        return 1;
    }

    rte_mempool_put(msg_pool, (void *)conn_msg);

    fprintf(stdout, "Disconnected.\n");
    return 0;
}

int32_t
send_msg(char *__msg, int32_t __dst) 
{
    struct rte_mempool *msg_pool = get_message_pool();
    uint8_t client_id = get_id();
    struct ipc_message *imsg;
    struct ipc_message *ack_msg;

    rte_mempool_get(msg_pool, (void **)&imsg);
    if (imsg == NULL) {
        fprintf(stderr, "Cannot obtain memory for connection message.\n");
        return -1;
    }

    imsg->m_type = DISCONNECT;
    sprintf(imsg->m_dtbuf, "%d-%d-%s", client_id, __dst, __msg);

    if (rte_ring_enqueue(comm_rx_ring, imsg) < 0) {
        fprintf(stderr, "Could not send disconnect request.\n");
        return -1;
    }

    // /**
    //  * Check if the ack is received
    //  */
    // while (rte_ring_dequeue(conn_tx_ring, (void **)&ack_msg) < 0);
    // if (ack_msg == NULL) {
    //     fprintf(stderr, "Could not receive connection ack.\n");
    //     return -1;
    // }

    // /**
    //  * Check if ack is correct
    //  */
    // if ((imsg->m_type != ack_msg->m_type) || strcmp(imsg->m_dtbuf, ack_msg->m_dtbuf)) {
    //     fprintf(stderr, "Received bad ack.\n");
    //     return 1;
    // }

    rte_mempool_put(msg_pool, (void *)imsg);

    // fprintf(stdout, "Disconnected.\n");

    return 0;
}

void
send_data(void *data, uint16_t size, uint16_t out_port)
{
    __rte_unused int sent;
    uint8_t client_id = get_id();
    struct rte_mempool *pkt_pool = get_pktbuf_mempool();
    void *app_data;

    struct rte_mbuf *buf = rte_pktmbuf_alloc(pkt_pool);
    if (buf == NULL)
        rte_exit(EXIT_FAILURE, "Coul not obtain an mbuf from the pool.\n");

    // rte_pktmbuf_read(buf, 0, size, data);
    app_data = rte_pktmbuf_append(buf, size);
    if (app_data == NULL) {
        rte_exit(EXIT_FAILURE, "Could not append data to the mbuf.\n");
    }

    fprintf(stdout, "Put data in the mbuf.\n");
    memcpy(app_data, data, size);
    buf->buf_len = size;
    buf->pkt_len = size;

    fprintf(stdout, "Sending information.\n");
    // sent = rte_eth_tx_buffer(out_port, client_id, tx_buffer, buf);
    // sent = rte_eth_tx_buffer(out_port, client_id, tx_buffer, buf);
    // sent = rte_eth_tx_buffer(out_port, client_id, tx_buffer, buf);
    // sent = rte_eth_tx_buffer(out_port, client_id, tx_buffer, buf);
    // sent = rte_eth_tx_buffer(out_port, client_id, tx_buffer, buf);
    // sent = rte_eth_tx_buffer(out_port, client_id, tx_buffer, buf);
    // sent = rte_eth_tx_buffer(out_port, client_id, tx_buffer, buf);
    // sent = rte_eth_tx_buffer(out_port, client_id, tx_buffer, buf);
    // sent = rte_eth_tx_buffer(out_port, client_id, tx_buffer, buf);
    // sent = rte_eth_tx_buffer(out_port, client_id, tx_buffer, buf);
    // sent = rte_eth_tx_buffer(out_port, client_id, tx_buffer, buf);
    // sent = rte_eth_tx_buffer(out_port, client_id, tx_buffer, buf);
    // sent = rte_eth_tx_buffer(out_port, client_id, tx_buffer, buf);
    // sent = rte_eth_tx_buffer(out_port, client_id, tx_buffer, buf);
    // sent = rte_eth_tx_buffer(out_port, client_id, tx_buffer, buf);
    // sent = rte_eth_tx_buffer(out_port, client_id, tx_buffer, buf);
    // sent = rte_eth_tx_buffer(out_port, client_id, tx_buffer, buf);

    sent = rte_eth_tx_burst(out_port, client_id, &buf, 1);

    // sent = rte_eth_tx_buffer_flush(client_id, client_id, tx_buffer);
    if (sent)
		fprintf(stdout, "Sent %hu packets to Ehternet.\n", sent);
}

void
receive_data(void)
{
    void *pkts[PKT_READ_SIZE];
    uint16_t rx_pkts;

    while(true) {
        rx_pkts = rte_ring_dequeue_burst(rx_ring, pkts, PKT_READ_SIZE, NULL);
        if (rx_pkts == 0)
            continue;
        fprintf(stdout, "Received some information\n");
    }
}
