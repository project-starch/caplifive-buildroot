#include <stdio.h>
#include <stdlib.h>

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

#include "init.h"
#include "args_parser.h"
#include "commons.h"

static struct rte_mempool *pktmbuf_pool;
static struct client *clients;

static struct cclient *cclients = NULL;

struct rte_mempool *message_pool;

static struct port_info *ports = NULL;

/**
 * This ring is going to be set up to receive new connection requests 
 * from the clients (secondary processes)
*/
static struct rte_ring *conn_rx_ring = NULL;

/**
 * This ring is used to send connection responses
*/
static struct rte_ring *conn_tx_ring = NULL;

static const uint32_t conn_ring_size = 128;
static const uint32_t msg_pool_size = 1024;

struct port_info *
get_ports(void)
{
    return ports;
}

struct rte_ring *
get_conn_rx_ring(void)
{
	return conn_rx_ring;
}

struct rte_ring *
get_conn_tx_ring(void)
{
	return conn_tx_ring;
}

struct rte_mempool *
get_msg_pool(void)
{
	return message_pool;
}

struct client *
get_clients(void)
{
	return clients;
}

struct cclient *
get_cclients(void)
{
	return cclients;
}

/**
 * Initialise the mbuf pool for packet reception for the NIC, and any other
 * buffer pools needed by the app - currently none.
 */
static int
init_mbuf_pools(void)
{
    uint8_t num_clients = get_num_clients();

	const unsigned int num_mbufs_server =
		RTE_MP_RX_DESC_DEFAULT * ports->num_ports;
	const unsigned int num_mbufs_client =
		num_clients * (CLIENT_QUEUE_RINGSIZE +
			       RTE_MP_TX_DESC_DEFAULT * ports->num_ports);
	const unsigned int num_mbufs_mp_cache =
		(num_clients + 1) * MBUF_CACHE_SIZE;
	const unsigned int num_mbufs =
		num_mbufs_server + num_mbufs_client + num_mbufs_mp_cache;

	/* don't pass single-producer/single-consumer flags to mbuf create as it
	 * seems faster to use a cache instead */
	printf("Creating mbuf pool '%s' [%u mbufs] ...\n",
			PKTMBUF_POOL_NAME, num_mbufs);
	pktmbuf_pool = rte_pktmbuf_pool_create(PKTMBUF_POOL_NAME, num_mbufs,
		MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

	return pktmbuf_pool == NULL; /* 0  on success */
}

/**
 * Initialise an individual port:
 * - configure number of rx and tx rings
 * - set up each rx ring, to pull from the main mbuf pool
 * - set up each tx ring
 * - start the port and report its status to stdout
 */
static int
init_port(uint16_t port_num)
{
	uint8_t num_clients = get_num_clients();

	/* for port configuration all features are off by default */
	const struct rte_eth_conf port_conf = {
		// .rxmode = {
		// 	.mq_mode = RTE_ETH_MQ_RX_RSS
		// }
		.rxmode = {
			.mq_mode = RTE_ETH_MQ_RX_NONE,
		},
		// .rx_adv_conf = {
		// 	.rss_conf = {
		// 		.rss_key = NULL,
		// 		.rss_key_len = 40,
		// 		.rss_hf = 0,
		// 	},
		// },
		.txmode = {
			.mq_mode = RTE_ETH_MQ_TX_NONE,
		}
	};

	const uint16_t rx_rings = 1, tx_rings = num_clients;
	uint16_t rx_ring_size = RTE_MP_RX_DESC_DEFAULT;
	uint16_t tx_ring_size = RTE_MP_TX_DESC_DEFAULT;

	uint16_t q;
	int retval;

	printf("Port %u init ...", port_num);
	fflush(stdout);

	/* Standard DPDK port initialisation - config port, then set up
	 * rx and tx rings */
	if ((retval = rte_eth_dev_configure(port_num, rx_rings, tx_rings,
		&port_conf)) != 0)
		return retval;

	retval = rte_eth_dev_adjust_nb_rx_tx_desc(port_num, &rx_ring_size,
			&tx_ring_size);
	if (retval != 0)
		return retval;

	for (q = 0; q < rx_rings; q++) {
		retval = rte_eth_rx_queue_setup(port_num, q, rx_ring_size,
				rte_eth_dev_socket_id(port_num),
				NULL, pktmbuf_pool);
		if (retval < 0) return retval;
	}

	for ( q = 0; q < tx_rings; q ++ ) {
		retval = rte_eth_tx_queue_setup(port_num, q, tx_ring_size,
				rte_eth_dev_socket_id(port_num),
				NULL);
		if (retval < 0) return retval;
	}

	retval = rte_eth_promiscuous_enable(port_num);
	if (retval < 0)
		return retval;

	retval  = rte_eth_dev_start(port_num);
	if (retval < 0) return retval;

	printf( "done: \n");

	return 0;
}

/**
 * Set the DPDK ring for the server (main process).
 * This ring is going to be used by each client (secondary process) in order to announce its 
 * intention to be a part of the communication.
*/
static int32_t
init_connection_rings(void)
{
	int32_t ret = 0;

	conn_rx_ring = rte_ring_create(CONN_RX_RING, conn_ring_size, rte_socket_id(), RING_F_SC_DEQ);
	if (conn_rx_ring == NULL)
		rte_exit(EXIT_FAILURE, "Could not create connection ring.\n");

	conn_tx_ring = rte_ring_create(CONN_TX_RING, conn_ring_size, rte_socket_id(), RING_F_SP_ENQ);
	if (conn_tx_ring == NULL)
		rte_exit(EXIT_FAILURE, "Could not create connection ring.\n");

	return (ret);	
}

/**
 * Set up the DPDK rings which will be used to pass packets, via
 * pointers, between the multi-process server and client processes.
 * Each client needs one RX queue.
 */
static int
init_shm_rings(void)
{
	unsigned i;
	unsigned socket_id;
	const char * q_name;
	const unsigned ringsize = CLIENT_QUEUE_RINGSIZE;
	uint8_t num_clients = get_num_clients();

	clients = rte_malloc("client details", sizeof(*clients) * num_clients, 0);
	if (clients == NULL)
		rte_exit(EXIT_FAILURE, "Cannot allocate memory for client program details\n");

	for (i = 0; i < num_clients; i++) {
		/* Create an RX queue for each client */
		socket_id = rte_socket_id();
		q_name = get_rx_queue_name(i);
		clients[i].rx_q = rte_ring_create(q_name,
				ringsize, socket_id,
				RING_F_SP_ENQ | RING_F_SC_DEQ ); /* single prod, single cons */
		if (clients[i].rx_q == NULL)
			rte_exit(EXIT_FAILURE, "Cannot create rx ring queue for client %u\n", i);
	}
	return 0;
}

/* Check the link status of all ports in up to 9s, and print them finally */
static void
check_all_ports_link_status(uint16_t port_num, uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
	uint16_t portid;
	uint8_t count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;
	int ret;
	char link_status_text[RTE_ETH_LINK_MAX_STR_LEN];

	printf("\nChecking link status");
	fflush(stdout);
	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		all_ports_up = 1;
		for (portid = 0; portid < port_num; portid++) {
			if ((port_mask & (1 << ports->id[portid])) == 0)
				continue;
			memset(&link, 0, sizeof(link));
			ret = rte_eth_link_get_nowait(ports->id[portid], &link);
			if (ret < 0) {
				all_ports_up = 0;
				if (print_flag == 1)
					printf("Port %u link get failed: %s\n",
						portid, rte_strerror(-ret));
				continue;
			}
			/* print link status if flag set */
			if (print_flag == 1) {
				rte_eth_link_to_str(link_status_text,
					sizeof(link_status_text), &link);
				printf("Port %d %s\n",
				       ports->id[portid],
				       link_status_text);
				continue;
			}
			/* clear all_ports_up flag if any link down */
			if (link.link_status == RTE_ETH_LINK_DOWN) {
				all_ports_up = 0;
				break;
			}
		}
		/* after finally printing all link status, get out */
		if (print_flag == 1)
			break;

		if (all_ports_up == 0) {
			printf(".");
			fflush(stdout);
			rte_delay_ms(CHECK_INTERVAL);
		}

		/* set the print_flag if all ports up or timeout */
		if (all_ports_up == 1 || count == (MAX_CHECK_TIME - 1)) {
			print_flag = 1;
			printf("done\n");
		}
	}
}

static int
init_clients_information(void)
{
	uint8_t num_clients = get_num_clients();
	uint8_t i;

	cclients = rte_malloc("clients information", sizeof(*cclients) * num_clients, 0);
	if (cclients == NULL)
		rte_exit(EXIT_FAILURE, "Cannot allocate memory for client program details\n");

	for (i = 0; i < num_clients; ++i) {
		char *rxq = get_ring_name(COMM_RX_RING, i);
		if (rxq == NULL) {
			fprintf(stdout, "Could not initialize ring.\n");
			continue;
		}
		char *txq = get_ring_name(COMM_TX_RING, i);
		if (txq == NULL) {
			fprintf(stdout, "Could not initialize ring.\n");
			continue;
		}

		fprintf(stdout, "Initialing rx ring %s.\n", rxq);
		cclients[i].m_rx_q = rte_ring_create(rxq, conn_ring_size, rte_socket_id(), 0);
		if (cclients[i].m_rx_q == NULL)
			rte_exit(EXIT_FAILURE, "Could not create connection ring.\n");

		fprintf(stdout, "Initialing tx ring %s.\n", txq);
		cclients[i].m_tx_q = rte_ring_create(txq, conn_ring_size, rte_socket_id(), 0);
		if (cclients[i].m_tx_q == NULL)
			rte_exit(EXIT_FAILURE, "Could not create connection ring.\n");
	}

	return 0;
}

int32_t
init(int32_t __argc, char *__argv[])
{
    int retval;
	const struct rte_memzone *mz;
	// uint16_t i;

	/* init EAL, parsing EAL args */
	retval = rte_eal_init(__argc, __argv);
	if (retval < 0)
		return -1;
	__argc -= retval;
	__argv += retval;

	/* set up array for port data */
	mz = rte_memzone_reserve(MZ_PORT_INFO, sizeof(*ports),
				rte_socket_id(), NO_FLAGS);
	if (mz == NULL)
		rte_exit(EXIT_FAILURE, "Cannot reserve memory zone for port information\n");
	memset(mz->addr, 0, sizeof(*ports));
	ports = mz->addr;

	/* parse additional, application arguments */
	retval = parse_app_args(__argc, __argv);
	/**
	 * Disable the check below for now because the call from above will always fail
	 * because there are no network ports (interfaces) configured
	*/
	// if (retval != 0)
	// 	return -1;

	// /* initialise mbuf pools */
	// retval = init_mbuf_pools();
	// if (retval != 0)
	// 	rte_exit(EXIT_FAILURE, "Cannot create needed mbuf pools\n");

	// /* now initialise the ports we will use */
	// for (i = 0; i < ports->num_ports; i++) {
	// 	retval = init_port(ports->id[i]);
	// 	if (retval != 0)
	// 		rte_exit(EXIT_FAILURE, "Cannot initialise port %u\n",
	// 				(unsigned)i);
	// }

	// check_all_ports_link_status(ports->num_ports, (~0x0));

	// /* initialise the client queues/rings for inter-eu comms */
	// init_shm_rings();

	// init_connection_rings();

	// init_clients_information();

	message_pool = rte_mempool_create(MSG_POOL_NAME, msg_pool_size,
				MSG_SIZE, 64, 0,
				NULL, NULL, NULL, NULL,
				rte_socket_id(), 0);
	if (message_pool == NULL) {
		rte_exit(EXIT_FAILURE, "Could not initialise message pool.\n");
	}

	return 0;
}
