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
	pr_info("From C mode: %lx\n", sbi_res.value);
}

static long device_ioctl(struct file* file,
					     unsigned int ioctl_num,
						 unsigned long ioctl_param)
{
	switch (ioctl_num) {
		case IOCTL_DOM_CREATE:
			// printk(KERN_INFO "Hello!\n");
			// sbi_res = sbi_ecall(SBI_EXT_CAPSTONE, 0, 0, 0, 0, 0, 0, 0);
			// printk(KERN_INFO "From C mode: %ld\n", sbi_res.value);
			create_dom((struct ioctl_dom_create_args* __user)ioctl_param);

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
