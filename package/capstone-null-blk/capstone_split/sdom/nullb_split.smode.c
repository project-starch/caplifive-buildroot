#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include "nullb_split.smode.h"

#define C_PRINT(v) __asm__ volatile(".insn r 0x5b, 0x1, 0x43, x0, %0, x0" :: "r"(v))

#define DEBUG_COUNTER_SHARED 10
#define DEBUG_COUNTER_SHARED_TIMES 11
#define DEBUG_COUNTER_BORROWED 12
#define DEBUG_COUNTER_BORROWED_TIMES 13
#define debug_counter_inc(counter_no, delta) __asm__ volatile(".insn r 0x5b, 0x1, 0x45, x0, %0, %1" :: "r"(counter_no), "r"(delta))
#define debug_shared_counter_inc(delta) debug_counter_inc(DEBUG_COUNTER_SHARED, delta); debug_counter_inc(DEBUG_COUNTER_SHARED_TIMES, 1)
#define debug_borrowed_counter_inc(delta) debug_counter_inc(DEBUG_COUNTER_BORROWED, delta); debug_counter_inc(DEBUG_COUNTER_BORROWED_TIMES, 1)

#define METADATA_REGION_ID 1

static char stack[4096];

#define REGION_ID_TO_BASE(region_id) \
	(char *)(sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_QUERY, \
		/* region_id = */ region_id, \
		/* field = */ CAPSTONE_REGION_FIELD_BASE, \
		0, 0, 0, 0).value);

static void nullbs_null_validate_conf(void)
{
	struct sbiret sbi_res = sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_COUNT,
		0, 0, 0, 0, 0, 0);
	const region_id_t region_n = sbi_res.value;
	region_id_t wo_region = region_n - 1;
	char *wo_region_base = REGION_ID_TO_BASE(wo_region);
	region_id_t nullb_dev_region = wo_region - 1;
	char *nullb_dev_region_base = REGION_ID_TO_BASE(nullb_dev_region);
	
	int rv;
	struct nullb_device *dev = (struct nullb_device *)nullb_dev_region_base;

	dev->blocksize = round_down(dev->blocksize, 512);
	dev->blocksize = clamp_t(unsigned int, dev->blocksize, 512, 4096);

	if (dev->queue_mode == NULL_Q_MQ && dev->use_per_node_hctx) {
		if (dev->submit_queues != nr_online_nodes)
			dev->submit_queues = nr_online_nodes;
	} else if (dev->submit_queues > qemu_nr_cpu_ids)
		dev->submit_queues = qemu_nr_cpu_ids;
	else if (dev->submit_queues == 0)
		dev->submit_queues = 1;
	dev->prev_submit_queues = dev->submit_queues;

	if (dev->poll_queues > g_poll_queues)
		dev->poll_queues = g_poll_queues;
	dev->prev_poll_queues = dev->poll_queues;

	dev->queue_mode = min_t(unsigned int, dev->queue_mode, NULL_Q_MQ);
	dev->irqmode = min_t(unsigned int, dev->irqmode, NULL_IRQ_TIMER);

	/* Do memory allocation, so set blocking */
	if (dev->memory_backed)
		dev->blocking = true;
	else /* cache is meaningless */
		dev->cache_size = 0;
	dev->cache_size = min_t(unsigned long, ULONG_MAX / 1024 / 1024,
						dev->cache_size);
	dev->mbps = min_t(unsigned int, 1024 * 40, dev->mbps);
	/* can not stop a queue */
	if (dev->queue_mode == NULL_Q_BIO)
		dev->mbps = 0;

	if (dev->zoned &&
	    (!dev->zone_size || !is_power_of_2(dev->zone_size))) {
		// pr_err("zone_size must be power-of-two\n");
		rv = -EINVAL;
		*((int *)(wo_region_base)) = rv;

		debug_borrowed_counter_inc(sizeof(int));
		debug_borrowed_counter_inc(sizeof(struct nullb_device));

		return;
	}

	rv = 0;
	*((int *)(wo_region_base)) = rv;

	debug_borrowed_counter_inc(sizeof(int));
	debug_borrowed_counter_inc(sizeof(struct nullb_device));

	sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_DE_LINEAR,
		wo_region, 0, 0, 0, 0, 0);
	sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_DE_LINEAR,
		nullb_dev_region, 0, 0, 0, 0, 0);

	sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_POP,
		2, 0, 0, 0, 0, 0);

	return;
}

static void nullbs_nullb_to_queue(void)
{
	struct sbiret sbi_res = sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_COUNT,
		0, 0, 0, 0, 0, 0);
	const region_id_t region_n = sbi_res.value;
	region_id_t wo_region = region_n - 1;
	char *wo_region_base = REGION_ID_TO_BASE(wo_region);
	region_id_t ro_region = wo_region - 1;
	char *ro_region_base = REGION_ID_TO_BASE(ro_region);

	struct nullb *nullb = (struct nullb *)ro_region_base;
	
	int index = 0;

	if (nullb->nr_queues != 1)
		index = raw_smp_processor_id() / ((qemu_nr_cpu_ids + nullb->nr_queues - 1) / nullb->nr_queues);

	struct nullb_queue *rv = nullb->queues + index;
	*((struct nullb_queue **)(wo_region_base)) = rv;

	debug_borrowed_counter_inc(sizeof(struct nullb_queue *));

	sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_DE_LINEAR,
		wo_region, 0, 0, 0, 0, 0);

	sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_POP,
		2, 0, 0, 0, 0, 0);

	return;
}

static void nullbs_bio_op(void)
{
	struct sbiret sbi_res = sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_COUNT,
		0, 0, 0, 0, 0, 0);
	const region_id_t region_n = sbi_res.value;
	region_id_t wo_region = region_n - 1;
	char *wo_region_base = REGION_ID_TO_BASE(wo_region);
	region_id_t ro_region = wo_region - 1;
	char *ro_region_base = REGION_ID_TO_BASE(ro_region);

	struct bio *bio = (struct bio *)ro_region_base;

	enum req_op rv = bio->bi_opf & REQ_OP_MASK;
	
	*((enum req_op *)(wo_region_base)) = rv;

	debug_borrowed_counter_inc(sizeof(enum req_op));

	sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_DE_LINEAR,
		wo_region, 0, 0, 0, 0, 0);
	
	sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_POP,
		2, 0, 0, 0, 0, 0);
	
	return;
}

static void nullbs_end_cmd_bio(void)
{
	struct sbiret sbi_res = sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_COUNT,
		0, 0, 0, 0, 0, 0);
	const region_id_t region_n = sbi_res.value;
	region_id_t wo_region = region_n - 1;
	char *wo_region_base = REGION_ID_TO_BASE(wo_region);
	region_id_t ro_region = wo_region - 1;
	char *ro_region_base = REGION_ID_TO_BASE(ro_region);
	
	struct nullb_cmd *cmd = (struct nullb_cmd *)ro_region_base;

	*((int *)(wo_region_base)) = cmd->error;

	debug_borrowed_counter_inc(sizeof(int));

	sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_DE_LINEAR,
		wo_region, 0, 0, 0, 0, 0);

	sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_POP,
		2, 0, 0, 0, 0, 0);
	return;
}

static __attribute__((naked)) int __init nullb_split_init(void)
{
	__asm__ volatile ("mv sp, %0" :: "r"(stack + 4096));
	
	while(1) {
		unsigned long rv = 0;

		region_id_t metadata_region = METADATA_REGION_ID;
		char *metadata_region_base = REGION_ID_TO_BASE(metadata_region);

		unsigned long function_code = *((unsigned long *)metadata_region_base);
		debug_shared_counter_inc(sizeof(unsigned long));

        switch (function_code) {
            case NULLBS_NULL_VALIDATE_CONF:
                nullbs_null_validate_conf();
                break;
            case NULLBS_NULLB_TO_QUEUE:
                nullbs_nullb_to_queue();
                break;
            case NULLBS_BIO_OP:
                nullbs_bio_op();
                break;
            case NULLBS_END_CMD_BIO:
                nullbs_end_cmd_bio();
                break;
            default:
                rv = -1;
                break;
        }
        
		sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_DOM_RETURN,
			rv, 0, 0, 0, 0, 0);
	}

	return 0;
}

static void __exit nullb_split_exit(void)
{
	return;
}

module_init(nullb_split_init);
module_exit(nullb_split_exit);

MODULE_LICENSE("GPL");
