#pragma once

#include <cmdline_rdline.h>
#include <cmdline_parse.h>
#include <cmdline_parse_string.h>
#include <cmdline_socket.h>
#include <cmdline.h>

struct cmd_send_res {
    cmdline_fixed_string_t m_action;
    cmdline_fixed_string_t m_receiver;
    cmdline_multi_string_t m_msg;
};

struct cmd_send_to_domain {
    cmdline_fixed_string_t m_action;
    cmdline_fixed_string_t m_domain_id;
    cmdline_multi_string_t m_msg_arr;
};

struct cmd_receive_from_domain {
    cmdline_fixed_string_t m_action;
    cmdline_fixed_string_t m_domain_id;
};

__rte_unused static struct rte_ring *send_ring, *recv_ring;

void start_cmdline(void);
bool stop_server(void);
