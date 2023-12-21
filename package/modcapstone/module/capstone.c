#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/printk.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <asm/sbi.h>
#include <asm/errno.h>
#include "../include/capstone.h"
#include "capstone-sbi.h"

#define MAJOR_NUM 190
#define DEVICE_NAME "capstone"
#define DEVICE_FILE_NAME "capstone"

#define DOMAIN_DATA_SIZE (4096 * 4)

#define SUCCESS 0

static int device_open(struct inode *inode, struct file *file) {
	try_module_get(THIS_MODULE);
	return SUCCESS;
}

static int device_release(struct inode *inode, struct file *file) {
	module_put(THIS_MODULE);
	return SUCCESS;
}

static ssize_t device_read(struct file *file,
						   char __user *buffer,
						   size_t length,
						   loff_t *offset)
{
	// do nothing
	*offset = 0;
	return 0;
}

static ssize_t device_write(struct file *file,
						   const char __user *buffer,
						   size_t length,
						   loff_t *offset)
{
	// do nothing
	return 0;
}

static void create_dom(struct ioctl_dom_create_args* __user args) {
	struct ioctl_dom_create_args m_args;

	copy_from_user(&m_args, args, sizeof(struct ioctl_dom_create_args));

	// allocate a contiguous memory region and copy code there
	unsigned long dom_tot_size = m_args.code_len + DOMAIN_DATA_SIZE;
	unsigned long dom_pages = (dom_tot_size - 1) / PAGE_SIZE + 1;
	unsigned long dom_pages_log2 = dom_pages == 1 ? 0 : (ilog2(dom_pages - 1) + 1);

	unsigned long dom_vaddr = (unsigned long)__get_free_pages(GFP_HIGHUSER, dom_pages_log2);
	if (!dom_vaddr) {
		pr_alert("Failed to allocate memory for domain.\n");
		return;
	}

	unsigned long dom_paddr = __pa(dom_vaddr);
	// TODO: do we still need to do this on page granularity?
	pr_info("Domain memory region vaddr = %lx, paddr = %lx\n", dom_vaddr, dom_paddr);

	copy_from_user((void*)dom_vaddr, m_args.code_begin, m_args.code_len);

	struct sbiret sbi_res = sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_DOM_CREATE,
		/* base paddr = */ dom_paddr,
		/* code size = */ m_args.code_len,
		/* tot size = */ (1 << dom_pages_log2) * PAGE_SIZE, 
		/* entry offset = */ m_args.entry_offset,
		0, 0);
	m_args.dom_id = (dom_id_t)sbi_res.value;
	copy_to_user(args, &m_args, sizeof(struct ioctl_dom_create_args));

	if (m_args.s_code_len > 0) {
		// this domain has an S mode
		// call the domain with the code region to allow initialisation

		unsigned long dom_s_code_pages = (m_args.s_code_len - 1) / PAGE_SIZE + 1;
		unsigned long dom_s_code_pages_log2 = dom_s_code_pages == 1 ? 0 : (ilog2(dom_s_code_pages - 1) + 1);
		unsigned long dom_s_code_vaddr = (unsigned long)__get_free_pages(GFP_HIGHUSER, dom_s_code_pages_log2);
		if(!dom_s_code_pages) {
			pr_alert("Failed to allocate S-mode code region for domain.\n");
			return;
		}

		pr_info("Domain S-mode region vaddr = %lx, paddr = %lx\n", dom_s_code_vaddr, __pa(dom_s_code_vaddr));

		copy_from_user((void*)dom_s_code_vaddr, m_args.s_code_begin, m_args.s_code_len);

		sbi_res = sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_DOM_CALL_WITH_CAP,
			m_args.dom_id, __pa(dom_s_code_vaddr), m_args.s_code_len,
			__pa(dom_s_code_vaddr) + m_args.s_entry_offset, 0, 0);
		
		if (sbi_res.value) {
			pr_alert("Failed to initialise S mode\n");
		}
	}
}

static void call_dom(struct ioctl_dom_call_args* __user args) {
	struct ioctl_dom_call_args m_args;
	copy_from_user(&m_args, args, sizeof(struct ioctl_dom_call_args));

	struct sbiret sbi_res = sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_DOM_CALL,
				m_args.dom_id, 0, 0, 0, 0, 0);
	m_args.retval = sbi_res.value;

	copy_to_user(args, &m_args, sizeof(struct ioctl_dom_call_args));
}

static void create_region(struct ioctl_region_create_args* __user args) {
	struct ioctl_region_create_args m_args;
	copy_from_user(&m_args, args, sizeof(struct ioctl_region_create_args));

	unsigned long n_pages = (m_args.len - 1) / PAGE_SIZE + 1;
	unsigned long n_pages_log2 = n_pages == 1 ? 0 : (ilog2(n_pages - 1) + 1);

	unsigned long vaddr = (unsigned long)__get_free_pages(GFP_HIGHUSER, n_pages_log2);
	if(!vaddr) {
		pr_alert("Failed to allocate memory region.\n");
		return;
	}

	struct sbiret sbi_res = sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_CREATE,
				__pa(vaddr), m_args.len, 0, 0, 0, 0);
	m_args.region_id = sbi_res.value;

	copy_to_user(args, &m_args, sizeof(struct ioctl_region_create_args));
}

static void share_region(struct ioctl_region_share_args* __user args) {
	struct ioctl_region_share_args m_args;
	copy_from_user(&m_args, args, sizeof(struct ioctl_region_share_args));

	struct sbiret sbi_res = sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_SHARE,
				m_args.dom_id, m_args.region_id, 0, 0, 0, 0);
	m_args.retval = sbi_res.value;

	copy_to_user(args, &m_args, sizeof(struct ioctl_region_share_args));
}

static long device_ioctl(struct file* file,
					     unsigned int ioctl_num,
						 unsigned long ioctl_param)
{
	switch (ioctl_num) {
		case IOCTL_DOM_CREATE:
			create_dom((struct ioctl_dom_create_args* __user)ioctl_param);
			break;
		case IOCTL_DOM_CALL:
			call_dom((struct ioctl_dom_call_args* __user)ioctl_param);
			break;
		case IOCTL_REGION_CREATE:
		 	create_region((struct ioctl_region_create_args* __user)ioctl_param);
			break;
		case IOCTL_REGION_SHARE:
		 	share_region((struct ioctl_region_share_args* __user)ioctl_param);
			break;
		default:
			pr_info("Unrecognised IOCTL command %u\n", ioctl_num);
	}
	return 0;
}

static struct file_operations fops = {
	.read = device_read,
	.write = device_write,
	.unlocked_ioctl = device_ioctl,
	.open = device_open,
	.release = device_release
};

static struct class *cls;

static int __init capstone_init(void)
{
	int retval = register_chrdev(MAJOR_NUM, DEVICE_NAME, &fops);
	if (retval < 0) {
		pr_alert("Failed to register device major number %u for %s", MAJOR_NUM, DEVICE_NAME);
		return retval;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
	cls = class_create(DEVICE_FILE_NAME);
#else
	cls = class_create(THIS_MODULE, DEVICE_FILE_NAME);
#endif
	device_create(cls, NULL, MKDEV(MAJOR_NUM, 0), NULL, DEVICE_FILE_NAME);

	return 0;
}

static void __exit capstone_exit(void)
{
	device_destroy(cls, MKDEV(MAJOR_NUM, 0));
	class_destroy(cls);
	unregister_chrdev(MAJOR_NUM, DEVICE_NAME);
}


module_init(capstone_init);
module_exit(capstone_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Captainer kernel module");
