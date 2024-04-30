#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <getopt.h>
#include <stdarg.h>
#include <errno.h>

#include <rte_memory.h>
#include <rte_ethdev.h>
#include <rte_string_fns.h>
#include <rte_common.h>

#include <rte_string_fns.h>

#include "server_cmdline.h"
#include "libcapstone.h"
#include "capstone.h"

#include "commons.h"
#include "args_parser.h"

// #define __CAPSTONE_DEBUG_FLAG__

#define DEBUG_COUNTER_SHARED 10
#define DEBUG_COUNTER_SHARED_TIMES 11
#define DEBUG_COUNTER_BORROWED 12
#define DEBUG_COUNTER_BORROWED_TIMES 13
#define DEBUG_COUNTER_DOUBLE_TRANSFERRED 14
#define DEBUG_COUNTER_DOUBLE_TRANSFERRED_TIMES 15
#define DEBUG_COUNTER_BORROWED_TRANSFERRED 16
#define DEBUG_COUNTER_BORROWED_TRANSFERRED_TIMES 17
#define DEBUG_COUNTER_MUTABLE_BORROWED_TRANSFERRED 18
#define DEBUG_COUNTER_MUTABLE_BORROWED_TRANSFERRED_TIMES 19
#define debug_counter_inc(counter_no, delta) __asm__ volatile(".insn r 0x5b, 0x1, 0x45, x0, %0, %1" :: "r"(counter_no), "r"(delta))
#define debug_shared_counter_inc(delta) debug_counter_inc(DEBUG_COUNTER_SHARED, delta); debug_counter_inc(DEBUG_COUNTER_SHARED_TIMES, 1)
#define debug_borrowed_counter_inc(delta) debug_counter_inc(DEBUG_COUNTER_BORROWED, delta); debug_counter_inc(DEBUG_COUNTER_BORROWED_TIMES, 1)
#define debug_double_transferred_counter_inc(delta) debug_counter_inc(DEBUG_COUNTER_DOUBLE_TRANSFERRED, delta); debug_counter_inc(DEBUG_COUNTER_DOUBLE_TRANSFERRED_TIMES, 1)
#define debug_borrowed_transferred_counter_inc(delta) debug_counter_inc(DEBUG_COUNTER_BORROWED_TRANSFERRED, delta); debug_counter_inc(DEBUG_COUNTER_BORROWED_TRANSFERRED_TIMES, 1)
#define debug_mutable_borrowed_transferred_counter_inc(delta) debug_counter_inc(DEBUG_COUNTER_MUTABLE_BORROWED_TRANSFERRED, delta); debug_counter_inc(DEBUG_COUNTER_MUTABLE_BORROWED_TRANSFERRED_TIMES, 1)

static const char *_MSG_POOL = "MSG_POOL_SERVER";
static volatile bool quit = false;

bool
stop_server(void)
{
    return quit;
}

static struct rte_mempool *message_pool;

static void
send_to_client_domain(dom_id_t __dom_id, struct client_domain __client_domain, char *__msg_arr)
{
    dom_id_t dom_id = __client_domain.id;
    region_id_t metadata_region_id = __client_domain.metadata_region_id;
    region_id_t send_region_id = __client_domain.send_region_id;
    
    char *metadata_region_base = map_region(metadata_region_id, 4096);
    char *send_region_base = map_region(send_region_id, 4096);
    
    char *tok;
    unsigned nr_vals = 0;
    unsigned long long val;
    char *ptr;

    tok = strtok(__msg_arr, " ");

    /**
     * We construct an array to be sent to the client domain
    */
    printf("Array sent to client:");
    while (tok != NULL) {
        val = strtoul(tok, &ptr, 10);
        printf(" %x", val);
        *(unsigned long long *)(send_region_base + 8 * nr_vals) = val;
        nr_vals++;

        tok = strtok(NULL, " ");
    }
    printf("\n");

    /*fill up metadata*/
	*(unsigned long long *)metadata_region_base = SERVER_SEND;
	*(unsigned long long *)(metadata_region_base + 8) = nr_vals;
    debug_shared_counter_inc(2 * sizeof(unsigned long long));

    shared_region_annotated(dom_id, send_region_id, CAPSTONE_ANNOTATION_PERM_IN, CAPSTONE_ANNOTATION_REV_BORROWED);
    debug_borrowed_counter_inc(nr_vals * sizeof(unsigned long long));

	call_dom(__dom_id);
    revoke_region(send_region_id);

	/**
	 * Wait for client to process information
	*/
	while (*(unsigned long long *)metadata_region_base != ACK);
	fprintf(stdout, "Client processed %lu values.\n", nr_vals);
}

static void cmd_send_to_domain_parsed(void *parsed_result,
        __rte_unused struct cmdline *cl,
        __rte_unused void *data)
{
    uint8_t i;
    dom_id_t dom_id;
    char *ptr;
    struct client_domain client_domain;
    uint8_t num_clients = get_num_clients();
    struct cmd_send_to_domain *res = parsed_result;
    struct client_domain *client_domains = get_client_domains();

    dom_id = strtoul(res->m_domain_id, &ptr, 10);

    for (i = 0; i < num_clients; ++i) {
        if (client_domains[i].id == dom_id) {
            client_domain = client_domains[i];
            break;
        }
    }

    if (i >= num_clients) {
        fprintf(stderr, "Could not find the domain id %lu.\n", dom_id);
        return;
    }

    send_to_client_domain(dom_id, client_domain, res->m_msg_arr);
}

cmdline_parse_token_string_t cmd_send_to_domain_action =
    TOKEN_STRING_INITIALIZER(struct cmd_send_to_domain, m_action, "send_to_domain");
cmdline_parse_token_string_t cmd_send_to_domain_domain_id =
    TOKEN_STRING_INITIALIZER(struct cmd_send_to_domain, m_domain_id, NULL);
cmdline_parse_token_string_t cmd_send_to_domain_msg_arr =
    TOKEN_STRING_INITIALIZER(struct cmd_send_to_domain, m_msg_arr, TOKEN_STRING_MULTI);

cmdline_parse_inst_t cmd_send_to_domain = {
    .f = cmd_send_to_domain_parsed,  /* function to call */
    .data = NULL,      /* 2nd arg of func */
    .help_str = "send an array of values to a client domain",
    .tokens = {        /* token list, NULL terminated */
            (void *)&cmd_send_to_domain_action,
            (void *)&cmd_send_to_domain_domain_id,
            (void *)&cmd_send_to_domain_msg_arr,
            NULL,
    },
};


static void
receive_from_client_domain(dom_id_t __dom_id, struct client_domain __client_domain)
{
    unsigned nr_recv_vals = 0;

    region_id_t metadata_region_id = __client_domain.metadata_region_id;
    region_id_t receive_region_id = __client_domain.receive_region_id;
    
    char *metadata_region_base = map_region(metadata_region_id, 4096);
    char *receive_region_base = map_region(receive_region_id, 4096);

	*(unsigned long long *)metadata_region_base = SERVER_RECEIVE;
    debug_shared_counter_inc(sizeof(unsigned long long));

    /*region sharing*/
    shared_region_annotated(__dom_id, receive_region_id, CAPSTONE_ANNOTATION_PERM_OUT, CAPSTONE_ANNOTATION_REV_BORROWED);
    
    call_dom(__dom_id);

    revoke_region(receive_region_id);

	/**
	 * Wait for client to process information
	*/
	while (*(unsigned long long *)metadata_region_base != ACK);

    nr_recv_vals = *(unsigned long long *)(metadata_region_base + 8);

    fprintf(stdout, "Number of elements about to be consumed: %u.\n", nr_recv_vals);
    fprintf(stdout, "Received from client domain %u:", __dom_id);

	for (unsigned i = 0; i < nr_recv_vals; ++i) {
        fprintf(stdout, " %u", *(unsigned long long *)(receive_region_base + 8 * i));
	}

    fprintf(stdout, "\n");

	fprintf(stdout, "Server processed %lu values.\n", nr_recv_vals);
}

static void cmd_receive_from_domain_parsed(void *parsed_result,
        __rte_unused struct cmdline *cl,
        __rte_unused void *data)
{
    uint8_t i;
    dom_id_t dom_id;
    char *ptr;
    struct client_domain client_domain;
    uint8_t num_clients = get_num_clients();
    struct cmd_receive_from_domain *res = parsed_result;
    struct client_domain *client_domains = get_client_domains();

    dom_id = strtoul(res->m_domain_id, &ptr, 10);

    for (i = 0; i < num_clients; ++i) {
        if (client_domains[i].id == dom_id) {
            client_domain = client_domains[i];
            break;
        }
    }

    if (i >= num_clients) {
        fprintf(stderr, "Could not find the domain id %lu.\n", dom_id);
        return;
    }

    receive_from_client_domain(dom_id, client_domain);
}

cmdline_parse_token_string_t cmd_receive_from_domain_action =
    TOKEN_STRING_INITIALIZER(struct cmd_receive_from_domain, m_action, "receive_from_domain");
cmdline_parse_token_string_t cmd_receive_from_domain_domain_id =
    TOKEN_STRING_INITIALIZER(struct cmd_receive_from_domain, m_domain_id, NULL);

cmdline_parse_inst_t cmd_receive_from_domain = {
    .f = cmd_receive_from_domain_parsed,  /* function to call */
    .data = NULL,      /* 2nd arg of func */
    .help_str = "receive an array of values from a client domain",
    .tokens = {        /* token list, NULL terminated */
        (void *)&cmd_receive_from_domain_action,
        (void *)&cmd_receive_from_domain_domain_id,
        NULL,
    },
};

struct cmd_quit_result {
    cmdline_fixed_string_t quit;
};

static void cmd_quit_parsed(__rte_unused void *parsed_result,
                struct cmdline *cl,
                __rte_unused void *data)
{
    quit = true;
    rte_mempool_free(message_pool);
    cmdline_quit(cl);
}

cmdline_parse_token_string_t cmd_quit_quit =
    TOKEN_STRING_INITIALIZER(struct cmd_quit_result, quit, "quit");

cmdline_parse_inst_t cmd_quit = {
    .f = cmd_quit_parsed,  /* function to call */
    .data = NULL,      /* 2nd arg of func */
    .help_str = "close the application",
    .tokens = {        /* token list, NULL terminated */
        (void *)&cmd_quit_quit,
        NULL,
    },
};

/****** CONTEXT (list of instruction) */
cmdline_parse_ctx_t simple_mp_ctx[] = {
    (cmdline_parse_inst_t *)&cmd_send_to_domain,
    (cmdline_parse_inst_t *)&cmd_receive_from_domain,
    (cmdline_parse_inst_t *)&cmd_quit,
    NULL,
};

void
start_cmdline(void)
{
    const unsigned flags = 0;
    const unsigned ring_size = 64;
    const unsigned pool_size = 1024;
    const unsigned pool_cache = 32;
    const unsigned priv_data_sz = 0;

    message_pool = rte_mempool_create(_MSG_POOL, pool_size,
                STR_TOKEN_SIZE, pool_cache, priv_data_sz,
                NULL, NULL, NULL, NULL,
                rte_socket_id(), flags);

    if (message_pool == NULL)
        rte_exit(EXIT_FAILURE, "Problem getting message pool\n");

    /* call cmd prompt on main lcore */
    struct cmdline *cl = cmdline_stdin_new(simple_mp_ctx, "\nserver_cmdline > ");
    if (cl == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create cmdline instance\n");

    cmdline_interact(cl);
    cmdline_stdin_exit(cl);
}
