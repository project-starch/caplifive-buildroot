/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdarg.h>
#include <inttypes.h>
#include <sys/queue.h>
#include <errno.h>
#include <signal.h>

#include <rte_common.h>
#include <rte_memory.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_ring.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_mempool.h>
#include <rte_memcpy.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_interrupts.h>
#include <rte_ethdev.h>
#include <rte_byteorder.h>
#include <rte_malloc.h>
#include <rte_string_fns.h>

#include "commons.h"
#include "init.h"
#include "args_parser.h"
#include "server_cmdline.h"

/**
 * CAPSTONE
*/
#include "capstone.h"
#include "libcapstone.h"
/**
 * CAPSTONE
*/

/*
 * When doing reads from the NIC or the client queues,
 * use this batch size
 */
#define PACKET_READ_SIZE 42

/*
 * Local buffers to put packets in, used to send packets in bursts to the
 * clients
 */
struct client_rx_buf {
	struct rte_mbuf *buffer[PACKET_READ_SIZE];
	uint16_t count;
};

/* One buffer per client rx queue - dynamically allocate array */
static struct client_rx_buf *cl_rx_buf;

static uint16_t connections[RTE_MAX_LCORE];

/*
 * send a burst of traffic to a client, assuming there are packets
 * available to be sent to this client
 */
__rte_unused static void
flush_rx_queue(uint16_t client)
{
	uint16_t j;
	struct client *cl;
	struct client *clients = get_clients();

	if (cl_rx_buf[client].count == 0)
		return;

	cl = &clients[client];
	if (rte_ring_enqueue_bulk(cl->rx_q, (void **)cl_rx_buf[client].buffer,
			cl_rx_buf[client].count, NULL) == 0){
		for (j = 0; j < cl_rx_buf[client].count; j++)
			rte_pktmbuf_free(cl_rx_buf[client].buffer[j]);
		cl->stats.rx_drop += cl_rx_buf[client].count;
	}
	else
		cl->stats.rx += cl_rx_buf[client].count;

	cl_rx_buf[client].count = 0;
}

/*
 * marks a packet down to be sent to a particular client process
 */
__rte_unused static inline void
enqueue_rx_packet(uint8_t client, struct rte_mbuf *buf)
{
	cl_rx_buf[client].buffer[cl_rx_buf[client].count++] = buf;
}

/*
 * This function takes a group of packets and routes them
 * individually to the client process. Very simply round-robins the packets
 * without checking any of the packet contents.
 */
static void
process_packets(uint32_t port_num __rte_unused,
		__rte_unused struct rte_mbuf *pkts[], uint16_t rx_count)
{
	uint16_t i;
	static uint8_t client;
	uint8_t num_clients = get_num_clients();
	void *buf;
	FILE *dump_file;
	dump_file = fopen("dump_file.txt", "a");

	for (i = 0; i < rx_count; i++) {
		rte_pktmbuf_dump(dump_file, pkts[i], pkts[i]->pkt_len);
		buf = calloc(pkts[i]->buf_len, 1);
		void *data = rte_pktmbuf_read(pkts[i], 0, pkts[i]->buf_len, buf);
		if (data != NULL) {
			fprintf(stdout, "Received from a client %s\n", (char *)data);
		}
		free(buf);
		// enqueue_rx_packet(client, pkts[i]);

		if (++client == num_clients)
			client = 0;
	}
	fclose(dump_file);

	// for (i = 0; i < num_clients; i++)
	// 	flush_rx_queue(i);
}

/*
 * Function called by the main lcore of the DPDK process.
 */
static int
do_packet_forwarding(__rte_unused void *arg)
{
	fprintf(stdout, "Starting function %s.\n", __func__);
	unsigned port_num = 0; /* indexes the port[] array */
	struct port_info *ports = get_ports();

	while(!stop_server()) {
		struct rte_mbuf *buf[PACKET_READ_SIZE];
		uint16_t rx_count;

		/* read a port */
		rx_count = rte_eth_rx_burst(ports->id[port_num], 0, buf, PACKET_READ_SIZE);
		// ports->rx_stats.rx[port_num] += rx_count;
		// rx_count = rte_eth_rx_burst(1, 0, buf, PACKET_READ_SIZE);  /** TODO: Remove this */

		/* Now process the NIC packets read */
		if (likely(rx_count > 0))
			process_packets(port_num, buf, rx_count);

		/* move to next port */
		++port_num;
		port_num %= ports->num_ports;
	}

	return 0;
}

static void
signal_handler(int signal)
{
	uint16_t port_id;

	if (signal == SIGINT)
		RTE_ETH_FOREACH_DEV(port_id) {
			rte_eth_dev_stop(port_id);
			rte_eth_dev_close(port_id);
		}
	exit(0);
}

static int
check_messages(__rte_unused void *__arg)
{
	fprintf(stdout, "Starting function %s.\n", __func__);
	struct cclient *cclients = get_cclients();
	uint8_t num_clients = get_num_clients();
	uint8_t i = 0;

	while (!stop_server()) {
		if (!connections[i])
			continue;

		struct ipc_message *conn_msg = NULL;

		if (rte_ring_dequeue(cclients[i].m_rx_q, (void **)&conn_msg) < 0) {
			usleep(5);
			i++;
			i %= num_clients;
			continue;
		}

		fprintf(stdout, "Received something from client %hhu.\n", i);

		i++;
		i %= num_clients;
	}

	return 0;
}

static int
listen_for_connections(__rte_unused void *arg)
{
	fprintf(stdout, "Starting function %s.\n", __func__);
	struct rte_ring *conn_rx_ring = get_conn_rx_ring();
	struct rte_ring *conn_tx_ring = get_conn_tx_ring();

	while (!stop_server()) {
		struct ipc_message *conn_msg = NULL;

		if (rte_ring_dequeue(conn_rx_ring, (void **)&conn_msg) < 0) {
			usleep(5);
			continue;
		}

		if (conn_msg != NULL) {
			int32_t requester_id = -1;
			sscanf(conn_msg->m_dtbuf, "%d", &requester_id);

			if (requester_id < 0)
				continue;

			if (conn_msg->m_type == CONNECT) {
				connections[requester_id] = true;

				/**
				 * Send back the same message to let the client know that connection is created
				 */
				if (rte_ring_enqueue(conn_tx_ring, (void *)conn_msg) < 0) {
					fprintf(stdout, "Could not send connection ack to %d.\n", requester_id);
					usleep(5);
					continue;
				}

				fprintf(stdout, "\nConnected id %d.\n", requester_id);

				continue;
			}

			if (conn_msg->m_type == DISCONNECT) {
				connections[requester_id] = false;
				if (rte_ring_enqueue(conn_tx_ring, (void *)conn_msg) < 0) {
					fprintf(stdout, "Could not send disconnect ack to %d.\n", requester_id);
				}

				fprintf(stdout, "\nDisconnecting %d...\n", requester_id);
			}
		}
	}

	return 0;
}

static void
init_client_domains()
{
	uint8_t num_clients = get_num_clients();
	uint8_t i;
	struct client_domain *client_domains = NULL;

	client_domains = calloc(num_clients, sizeof(struct client_domain));
	if (client_domains == NULL) {
		fprintf(stderr, "Could not reserve memory for client domains.\n");
		exit(1);
	}

	char *filename;
	dom_id_t dom_id;
	region_id_t region_id;

	for (i = 0; i < num_clients; ++i) {
		filename = NULL;
		asprintf(&filename, "/dpdk/dpdk_client%hhu.dom", i + 1);
		if (filename == NULL) {
			fprintf(stderr, "%d: Could not alloc memory for filename.", __LINE__);
			exit(EXIT_FAILURE);
		}

		dom_id = create_dom(filename, NULL);
		printf("Created domain from %s with ID = %lu\n", filename, dom_id);

		region_id = create_region(4096);
		char *region_base = map_region(region_id, 4096);
		memset(region_base, 0, 4096);
		share_region(dom_id, region_id);
		client_domains[i].id = dom_id;
		client_domains[i].region_base = region_base;

		free(filename);
	}

	set_client_domains(client_domains);
}

/* Initialization of Environment Abstraction Layer (EAL). 8< */
int
main(int __argc, char *__argv[])
{
	/**
	 * CAPSTONE
	*/
	int retval = 0;

	fprintf(stdout, "Capstone init\n");
    retval = capstone_init();
    if (retval) {
        return retval;
    }

    // dom_id_t dom_id;
    // dom_id = create_dom(file_name, NULL);
    // printf("Created domain ID = %lu\n", dom_id);

    // region_id_t region_id = create_region(4096);
    // char *region_base = map_region(region_id, 4096);
	// memset(region_base, 0, 4096);
    // share_region(dom_id, region_id);

    /**
     * Make memory domain vizible to the client processes
    */
    // set_domain_id(dom_id);

    // for (unsigned i = 1; i <= times; i ++) {
    //     unsigned long dom_retval = call_dom(dom_id);
    //     printf("Called dom (%u-th time) retval = %lu\n", i, dom_retval);
    // }


	/**
	 * CAPSTONE
	*/

    int ret = 0;
    // unsigned lcore_id;
	// uint16_t port_id;
	// uint32_t i = 0;
	// int (*fs[RTE_MAX_LCORE])(void *) = { 
	// 	listen_for_connections, 
	// 	do_packet_forwarding, 
	// 	check_messages 
	// };

    uint8_t num_clients = 0;
    // struct port_info *ports = NULL;

    signal(SIGINT, signal_handler);

    ret = init(__argc, __argv);
    if (ret < 0)
        goto end;

    num_clients = get_num_clients();
    // ports = get_ports();

    if (num_clients <= 0) {
        fprintf(stderr, "Invalid number of client %hhu.\n", num_clients);
        goto rte_eal_cleanup;
    }

	fprintf(stdout, "Initilized server with %hhu clients.\n", num_clients);
	init_client_domains();

    // if (ports == NULL) {
    //     fprintf(stderr, "Ports array could not be initialized.\n");
    //     goto rte_eal_cleanup;
    // }

	// fprintf(stdout, "Initilized server with %hhu clients and ports %p.\n", num_clients, ports);

	// /* Launches the functions on a separate lcore. 8< */
	// RTE_LCORE_FOREACH_WORKER(lcore_id) {
	// 	rte_eal_remote_launch(fs[i++], NULL, lcore_id);
	// }

	// send_to_client_domain(dom_id, region_base);
	start_cmdline();
	asm volatile (".insn r 0x5b, 0x1, 0x46, x0, x0, x0");

	rte_eal_mp_wait_lcore();

rte_eal_cleanup:

	// RTE_ETH_FOREACH_DEV(port_id) {
	// 	rte_eth_dev_stop(port_id);
	// 	rte_eth_dev_close(port_id);
	// }
    /* clean up the EAL */
    rte_eal_cleanup();

	retval = capstone_cleanup();

    if(retval) {
        fprintf(stderr, "Failed to clean up Capstone\n");
        return retval;
    }
end:
    return ret;
}
