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

#include "init.h"
#include "client_cmdline.h"

#include "communication.h"

static const char *_MSG_POOL = "MSG_POOL_CLIENT";
static struct rte_mempool *message_pool;
static bool quit = false;

bool
stop_client(void)
{
    return quit;
}

static void cmd_connect_parsed(__rte_unused void *parsed_result,
        __rte_unused struct cmdline *cl,
        __rte_unused void *data)
{
    request_connection();
}

cmdline_parse_token_string_t cmd_connect_action =
    TOKEN_STRING_INITIALIZER(struct cmd_connect_res, m_action, "connect");
cmdline_parse_token_string_t cmd_connection_extra = 
    TOKEN_STRING_INITIALIZER(struct cmd_connect_res, m_extra, TOKEN_STRING_MULTI);

cmdline_parse_inst_t cmd_connect = {
    .f = cmd_connect_parsed,
    .data = NULL,
    .help_str = "create communication with another process",
    .tokens = {
        (void *)&cmd_connect_action,
        NULL
    }
};

static void cmd_disconnect_parsed(__rte_unused void *parsed_result,
        __rte_unused struct cmdline *cl,
        __rte_unused void *data)
{
    disconnect();
}

cmdline_parse_token_string_t cmd_disconnect_action =
    TOKEN_STRING_INITIALIZER(struct cmd_disconnect_res, m_action, "disconnect");
cmdline_parse_token_string_t cmd_disconnection_extra = 
    TOKEN_STRING_INITIALIZER(struct cmd_disconnect_res, m_extra, TOKEN_STRING_MULTI);

cmdline_parse_inst_t cmd_disconnect = {
    .f = cmd_disconnect_parsed,
    .data = NULL,
    .help_str = "create communication with another process",
    .tokens = {
        (void *)&cmd_disconnect_action,
        NULL
    }
};

static void cmd_send_parsed(void *parsed_result,
        __rte_unused struct cmdline *cl,
        __rte_unused void *data)
{
    // void *msg = NULL;
    struct cmd_send_res *res = parsed_result;
    uint8_t client_id = get_id();

    // if (rte_mempool_get(message_pool, &msg) < 0)
    //     rte_panic("Failed to get message buffer\n");

    fprintf(stderr, "Sending %s to %s from %hhu.\n", res->m_msg, res->m_receiver, client_id);
    send_msg(res->m_msg, atoi(res->m_receiver));
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

struct cmd_quit_result {
    cmdline_fixed_string_t quit;
};

static void cmd_quit_parsed(__rte_unused void *parsed_result,
                struct cmdline *cl,
                __rte_unused void *data)
{
    disconnect();
    quit = true;
    rte_mempool_free(message_pool);
    cmdline_quit(cl);
}

cmdline_parse_token_string_t cmd_quit_quit =
    TOKEN_STRING_INITIALIZER(struct cmd_quit_result, quit, "quit");

cmdline_parse_inst_t cmd_quit = {
    .f = cmd_quit_parsed,  /* function to call */
    .data = NULL,      /* 2nd arg of func */
    .help_str = "close the client",
    .tokens = {        /* token list, NULL terminated */
        (void *)&cmd_quit_quit,
        NULL,
    }
};

/****** CONTEXT (list of instruction) */
cmdline_parse_ctx_t simple_mp_ctx[] = {
        (cmdline_parse_inst_t *)&cmd_send,
        (cmdline_parse_inst_t *)&cmd_connect,
        (cmdline_parse_inst_t *)&cmd_disconnect,
        (cmdline_parse_inst_t *)&cmd_quit,
        // (cmdline_parse_inst_t *)&cmd_help,
    NULL
};

void
start_cmdline(void)
{
    const unsigned flags = 0;
    // const unsigned ring_size = 64;
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
    struct cmdline *cl = cmdline_stdin_new(simple_mp_ctx, "\nclient_cmd > ");
    if (cl == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create cmdline instance\n");

    cmdline_interact(cl);
    cmdline_stdin_exit(cl);
}
