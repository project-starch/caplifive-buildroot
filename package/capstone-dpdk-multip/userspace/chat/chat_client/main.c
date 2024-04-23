#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <inttypes.h>
#include <stdio.h>
#include <termios.h>
#include <errno.h>
#include <sys/queue.h>

#include <rte_common.h>
#include <rte_memory.h>
#include <rte_eal.h>
#include <rte_branch_prediction.h>
#include <rte_launch.h>
#include <rte_log.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_ring.h>
#include <rte_debug.h>
#include <rte_mempool.h>
#include <rte_string_fns.h>

#include "init.h"
#include "client_cmdline.h"
#include "communication.h"

#include "commons.h"


#include "capstone.h"
#include "libcapstone.h"

/* Initialization of Environment Abstraction Layer (EAL). 8< */
int
main(int argc, char **argv)
{
    int ret;
    uint8_t client_id = 0;

    dom_id_t dom_id = get_domain_id();
    fprintf(stdout, "Got domain id %ld.\n", dom_id);

    /**
     * Make memory domain vizible to the client processes
    */
    set_domain_id(dom_id);

    unsigned long dom_retval = call_dom(dom_id);
    printf("Called dom (%u-th time) retval = %lu\n", 1, dom_retval);

    ret = init(argc, argv);
    if (ret < 0)
        rte_panic("Cannot init EAL\n");
    /* >8 End of initialization of Environment Abstraction Layer */

    client_id = get_id();

    RTE_LOG(INFO, EAL, "Initialized client with id %hhu.\n", client_id);

    // char *buf = "Hello my dear friend.";
    // send_data((void *)buf, strlen(buf), 1);
    start_cmdline();

    /* clean up the EAL */
    rte_eal_cleanup();

    return 0;
}
