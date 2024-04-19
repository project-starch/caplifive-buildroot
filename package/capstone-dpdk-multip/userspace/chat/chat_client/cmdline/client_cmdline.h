#pragma once

#include <cmdline_rdline.h>
#include <cmdline_parse.h>
#include <cmdline_parse_string.h>
#include <cmdline_socket.h>
#include <cmdline.h>

struct cmd_connect_res {
    cmdline_fixed_string_t m_action;
    cmdline_multi_string_t m_extra;  /** This is for future information */
};

struct cmd_disconnect_res {
    cmdline_fixed_string_t m_action;
    cmdline_multi_string_t m_extra;  /** This is for future information */
};

struct cmd_send_res {
    cmdline_fixed_string_t m_action;
    cmdline_fixed_string_t m_receiver;
    cmdline_multi_string_t m_msg;
};

bool stop_client(void);
void start_cmdline(void);
