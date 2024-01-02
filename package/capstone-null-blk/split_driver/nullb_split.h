#ifndef __NULL_BLK_SPLIT_H
#define __NULL_BLK_SPLIT_H

#include "../driver/null_blk.h"

static int g_poll_queues = 1;
// set nr_cpu_ids to 1 (as per qemu impl) to avoid undefined extern symbol
static unsigned int qemu_nr_cpu_ids = 1;

enum {
	NULL_IRQ_NONE		= 0,
	NULL_IRQ_SOFTIRQ	= 1,
	NULL_IRQ_TIMER		= 2,
};

enum nullb_device_flags {
	NULLB_DEV_FL_CONFIGURED	= 0,
	NULLB_DEV_FL_UP		= 1,
	NULLB_DEV_FL_THROTTLED	= 2,
	NULLB_DEV_FL_CACHE	= 3,
};

#endif /* __NULL_BLK_SPLIT_H */
