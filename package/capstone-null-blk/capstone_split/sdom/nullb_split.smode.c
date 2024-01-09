#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include "nullb_split.smode.h"

static char stack[4096];

static function_code_t *nullbs_fuction_code;
static char *nullbs_return_value;
static char *nullbs_shared_region;

static void nullbs_null_validate_conf(void)
{
	int *rv_ptr = (int *)nullbs_return_value;
	struct nullb_device *dev = (struct nullb_device *)nullbs_shared_region;

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
		*rv_ptr = -EINVAL;
		return;
	}

	*rv_ptr = 0;
	return;
}

static void nullbs_nullb_to_queue(void)
{
	struct nullb_queue **rv_ptr = (struct nullb_queue **)nullbs_return_value;
	struct nullb *nullb = (struct nullb *)nullbs_shared_region;

	int index = 0;

	if (nullb->nr_queues != 1)
		index = raw_smp_processor_id() / ((qemu_nr_cpu_ids + nullb->nr_queues - 1) / nullb->nr_queues);

	*rv_ptr = nullb->queues + index;
	return;
}

static void nullbs_bio_op(void)
{
	enum req_op *rv_ptr = (enum req_op *)nullbs_return_value;
	struct bio *bio = (struct bio *)nullbs_shared_region;

	*rv_ptr = bio->bi_opf & REQ_OP_MASK;
	return;
}

static void nullbs_end_cmd_bio(void)
{
	struct nullb_cmd *cmd = (struct nullb_cmd *)nullbs_shared_region;
	struct bio* bio = (struct bio*)(nullbs_shared_region + sizeof(struct nullb_cmd));

	bio->bi_status = cmd->error;
	return;
}

static void main(void) {
	while(1) {
		unsigned long rv = 0;

		struct sbiret sbi_res = sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_COUNT,
			0, 0, 0, 0, 0, 0);
		const unsigned long region_n = sbi_res.value;
		unsigned long region_shared_data = region_n - 1;
		unsigned long region_ret_val = region_n - 2;
		unsigned long region_func_code = region_n - 3;

		// function_code
		sbi_res = sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_QUERY,
			/* region_id = */ region_func_code,
			/* field = */ CAPSTONE_REGION_FIELD_BASE,
			0, 0, 0, 0);
		nullbs_fuction_code = (function_code_t *)sbi_res.value;
		function_code_t function_code = *nullbs_fuction_code;

		// nullbs_return_value
		sbi_res = sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_QUERY,
			/* region_id = */ region_ret_val,
			/* field = */ CAPSTONE_REGION_FIELD_BASE,
			0, 0, 0, 0);
		nullbs_return_value = (char *)sbi_res.value;

		// nullbs_shared_region
		sbi_res = sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_QUERY,
			/* region_id = */ region_shared_data,
			/* field = */ CAPSTONE_REGION_FIELD_BASE,
			0, 0, 0, 0);
		nullbs_shared_region = (char *)sbi_res.value;

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
}

// __attribute__((naked)) void _start() {
// 	__asm__ volatile ("mv sp, %0" :: "r"(stack + 4096));
// 	__asm__ volatile ("j main");
// }

static __attribute__((naked)) int __init nullb_split_init(void)
{
	__asm__ volatile ("mv sp, %0" :: "r"(stack + 4096));
	main();

	return 0;
}

static void __exit nullb_split_exit(void)
{
	return;
}

module_init(nullb_split_init);
module_exit(nullb_split_exit);

MODULE_LICENSE("GPL");
