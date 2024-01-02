#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include "nullb_split.h"

#define MEMORY_SIZE 4096
char *nullb_shared_region;
EXPORT_SYMBOL(nullb_shared_region);

// int null_validate_conf(struct nullb_device *dev)
// {
// 	dev->blocksize = round_down(dev->blocksize, 512);
// 	dev->blocksize = clamp_t(unsigned int, dev->blocksize, 512, 4096);

// 	if (dev->queue_mode == NULL_Q_MQ && dev->use_per_node_hctx) {
// 		if (dev->submit_queues != nr_online_nodes)
// 			dev->submit_queues = nr_online_nodes;
// 	} else if (dev->submit_queues > nr_cpu_ids)
// 		dev->submit_queues = nr_cpu_ids;
// 	else if (dev->submit_queues == 0)
// 		dev->submit_queues = 1;
// 	dev->prev_submit_queues = dev->submit_queues;

// 	if (dev->poll_queues > g_poll_queues)
// 		dev->poll_queues = g_poll_queues;
// 	dev->prev_poll_queues = dev->poll_queues;

// 	dev->queue_mode = min_t(unsigned int, dev->queue_mode, NULL_Q_MQ);
// 	dev->irqmode = min_t(unsigned int, dev->irqmode, NULL_IRQ_TIMER);

// 	/* Do memory allocation, so set blocking */
// 	if (dev->memory_backed)
// 		dev->blocking = true;
// 	else /* cache is meaningless */
// 		dev->cache_size = 0;
// 	dev->cache_size = min_t(unsigned long, ULONG_MAX / 1024 / 1024,
// 						dev->cache_size);
// 	dev->mbps = min_t(unsigned int, 1024 * 40, dev->mbps);
// 	/* can not stop a queue */
// 	if (dev->queue_mode == NULL_Q_BIO)
// 		dev->mbps = 0;

// 	if (dev->zoned &&
// 	    (!dev->zone_size || !is_power_of_2(dev->zone_size))) {
// 		pr_err("zone_size must be power-of-two\n");
// 		return -EINVAL;
// 	}

// 	return 0;
// }

int ns_null_validate_conf(void) {
	return 0;
}
EXPORT_SYMBOL(ns_null_validate_conf);

// struct nullb_queue *nullb_to_queue(struct nullb *nullb)
// {
// 	int index = 0;

// 	if (nullb->nr_queues != 1)
// 		index = raw_smp_processor_id() / ((nr_cpu_ids + nullb->nr_queues - 1) / nullb->nr_queues);

// 	return &nullb->queues[index];
// }

struct nullb_queue *ns_nullb_to_queue(void) {
	return NULL;
}
EXPORT_SYMBOL(ns_nullb_to_queue);

// enum req_op bio_op(const struct bio *bio)
// {
// 	return bio->bi_opf & REQ_OP_MASK;
// }

enum req_op ns_bio_op(void) {
	return 0;
}
EXPORT_SYMBOL(ns_bio_op);

// int null_cache_active(struct nullb *nullb)
// {
// 	return test_bit(NULLB_DEV_FL_CACHE, &nullb->dev->flags);
// }

int ns_null_cache_active(void) {
	return 0;
}
EXPORT_SYMBOL(ns_null_cache_active);

// void end_cmd(struct nullb_cmd *cmd)
// {
// 	int queue_mode = cmd->nq->dev->queue_mode;

// 	switch (queue_mode)  {
// 	case NULL_Q_MQ:
// 		blk_mq_end_request(cmd->rq, cmd->error);
// 		return;
// 	case NULL_Q_BIO:
// 		cmd->bio->bi_status = cmd->error;
// 		bio_endio(cmd->bio);
// 		break;
// 	}

// 	free_cmd(cmd);
// }

void ns_end_cmd(void) {
	return;
}
EXPORT_SYMBOL(ns_end_cmd);

static int __init nullb_split_init(void)
{
	printk(KERN_INFO "Entering Nullb Split module.");

	nullb_shared_region = kmalloc(MEMORY_SIZE, GFP_KERNEL);

    if (!nullb_shared_region) {
        printk(KERN_ERR "Failed to allocate memory.\n");
        return -ENOMEM;
    }

	return 0;
}

static void __exit nullb_split_exit(void)
{
	printk(KERN_INFO "Exiting Nullb Split module.");
}

module_init(nullb_split_init);
module_exit(nullb_split_exit);

MODULE_DESCRIPTION("Null block driver split module");
MODULE_LICENSE("GPL");
