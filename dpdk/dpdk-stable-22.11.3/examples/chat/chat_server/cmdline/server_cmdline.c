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

static const char *_MSG_POOL = "MSG_POOL_SERVER";
static volatile bool quit = false;

bool
stop_server(void)
{
    return quit;
}

static struct rte_mempool *message_pool;

static void cmd_send_parsed(void *parsed_result,
        __rte_unused struct cmdline *cl,
        __rte_unused void *data)
{
    void *msg = NULL;
    struct cmd_send_res *res = parsed_result;

    if (rte_mempool_get(message_pool, &msg) < 0)
        rte_panic("Failed to get message buffer\n");

    fprintf(stderr, "Sending %s to %s.\n", res->m_msg, res->m_receiver);

    // strlcpy((char *)msg, res->m_msg, STR_MULTI_TOKEN_SIZE);

    // if (rte_ring_enqueue(send_ring, msg) < 0) {
    //     printf("Failed to send message - message discarded\n");
    //     rte_mempool_put(message_pool, msg);
    // }
}

cmdline_parse_token_string_t cmd_send_action =
    TOKEN_STRING_INITIALIZER(struct cmd_send_res, m_action, "send");
cmdline_parse_token_string_t cmd_send_receiver =
    TOKEN_STRING_INITIALIZER(struct cmd_send_res, m_receiver, NULL);
cmdline_parse_token_string_t cmd_send_msg =
    TOKEN_STRING_INITIALIZER(struct cmd_send_res, m_msg, TOKEN_STRING_MULTI);

cmdline_parse_inst_t cmd_send = {
    .f = cmd_send_parsed,  /* function to call */
    .data = NULL,      /* 2nd arg of func */
    .help_str = "send a string to another process",
    .tokens = {        /* token list, NULL terminated */
            (void *)&cmd_send_action,
            (void *)&cmd_send_receiver,
            (void *)&cmd_send_msg,
            NULL,
    },
};

static void
send_to_client_domain(dom_id_t __dom_id, void *__region_base, char *__msg_arr)
{
    char *region_base = (char *)__region_base;
    char *tok;
    unsigned nr_vals = 0;
    unsigned long long val;
    char *ptr;

    tok = strtok(__msg_arr, " ");

    printf("Array sent to client:");
    while (tok != NULL) {
        val = strtoul(tok, &ptr, 10);
        printf(" %x", val);
        *(unsigned long long *)(region_base + 16 + 8 * nr_vals) = val;
        nr_vals++;

        tok = strtok(NULL, " ");
    }
    printf("\n");
    // uint8_t reg = 21;
    // asm volatile(".insn r 0x5b, 0x1, 0x45, x0, %0, %1"
    //         : "+r"(reg)
    //         : "r"(nr_vals * sizeof(long long)));
    asm volatile(".insn r 0x5b, 0x1, 0x45, x0, %0, %1"
            :
            : "r"(21), "r"(nr_vals * sizeof(long long)));

	/**
	 * Send information to the client domain
	*/
	*(unsigned long long *)region_base = CLIENT_PUT;
	*(unsigned long long *)(region_base + 8) = nr_vals;
	unsigned long return_dom = call_dom(__dom_id);

	/**
	 * Wait for client to process information
	*/
	while (*(unsigned long long *)region_base != ACK);
    // asm volatile(".insn r 0x5b, 0x1, 0x45, x0, %0, %1"
    //         :
    //         : "r"(23), "r"(1));
	fprintf(stdout, "Client processed %lu values.\n", return_dom);
}

static void cmd_send_to_domain_parsed(void *parsed_result,
        __rte_unused struct cmdline *cl,
        __rte_unused void *data)
{
    uint8_t i;
    dom_id_t dom_id;
    char *ptr;
    void *region_base = NULL;
    uint8_t num_clients = get_num_clients();
    struct cmd_send_to_domain *res = parsed_result;
    struct client_domain *client_domains = get_client_domains();

    dom_id = strtoul(res->m_domain_id, &ptr, 10);

    for (i = 0; i < num_clients; ++i) {
        if (client_domains[i].id == dom_id) {
            region_base = client_domains[i].region_base;
            break;
        }
    }

    if (region_base == NULL) {
        fprintf(stderr, "Could not find the domain id %lu.\n", dom_id);
        return;
    }

    send_to_client_domain(dom_id, region_base, res->m_msg_arr);
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
receive_from_client_domain(dom_id_t __dom_id, void *__region_base)
{
    unsigned nr_recv_vals = 0;
    char *region_base = (char *)__region_base;

	/**
	 * Send information to the client domain
	*/
	*(unsigned long long *)region_base = SERVER_GET;
	unsigned long long return_dom = call_dom(__dom_id);

	/**
	 * Wait for client to process information
	*/
	while (*(unsigned long long *)region_base != ACK);

    nr_recv_vals = *(unsigned long long *)(region_base + 8);
    // uint8_t reg = 22;
    // asm volatile(".insn r 0x5b, 0x1, 0x45, x0, %0, %1"
    //         : "+r"(reg)
    //         : "r"(nr_recv_vals * sizeof(long long)));
    asm volatile(".insn r 0x5b, 0x1, 0x45, x0, %0, %1"
            :
            : "r"(22), "r"(nr_recv_vals * sizeof(long long)));

    fprintf(stdout, "Number of elements about to be consumed: %u.\n", nr_recv_vals);
    fprintf(stdout, "Received from client domain %u:", __dom_id);
	for (unsigned i = 0; i < nr_recv_vals; ++i) {
        fprintf(stdout, " %u", *(unsigned long long *)(region_base + 16 + 8 * i));
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
    void *region_base = NULL;
    uint8_t num_clients = get_num_clients();
    struct cmd_receive_from_domain *res = parsed_result;
    struct client_domain *client_domains = get_client_domains();

    dom_id = strtoul(res->m_domain_id, &ptr, 10);

    for (i = 0; i < num_clients; ++i) {
        if (client_domains[i].id == dom_id) {
            region_base = client_domains[i].region_base;
            break;
        }
    }

    if (region_base == NULL) {
        fprintf(stderr, "Could not find the domain id %lu.\n", dom_id);
        return;
    }

    receive_from_client_domain(dom_id, region_base);
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
    (cmdline_parse_inst_t *)&cmd_send,
    (cmdline_parse_inst_t *)&cmd_send_to_domain,
    (cmdline_parse_inst_t *)&cmd_receive_from_domain,
    (cmdline_parse_inst_t *)&cmd_quit,
    // (cmdline_parse_inst_t *)&cmd_help,
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
