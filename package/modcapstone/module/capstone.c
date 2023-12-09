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

#define SBI_EXT_CAPSTONE 0x12345678

#define MAJOR_NUM 190
#define DEVICE_NAME "capstone"
#define DEVICE_FILE_NAME "capstone"

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
	unsigned long code_pages = (m_args.code_len - 1) / PAGE_SIZE + 1;
	unsigned long code_pages_log2 = code_pages == 1 ? 0 : (ilog2(code_pages - 1) + 1);
	
	unsigned long dom_code_vaddr = (unsigned long)__get_free_pages(GFP_HIGHUSER, code_pages_log2);
	if (!dom_code_vaddr) {
		pr_alert("Failed to allocate code region for domain.\n");
		return;
	}

	unsigned long dom_code_paddr = __pa(dom_code_vaddr);
	// TODO: do we still need to do this on page granularity?
	pr_info("Domain code vaddr = %lx, paddr = %lx\n", dom_code_vaddr, dom_code_paddr);

	// we just get one page for now
	unsigned long dom_data_vaddr = (unsigned long)__get_free_pages(GFP_HIGHUSER, 0);
	if (!dom_data_vaddr) {
		pr_alert("Failed to allocate data region for domain.\n");
		goto clean_up_code_region;
	}
	unsigned long dom_data_paddr = __pa(dom_data_vaddr);
	pr_info("Domain data vaddr = %lx, paddr = %lx\n", dom_data_vaddr, dom_data_paddr);

	return;
clean_up_code_region:
	free_pages(dom_code_vaddr, code_pages_log2);
}

static long device_ioctl(struct file* file,
					     unsigned int ioctl_num,
						 unsigned long ioctl_param)
{
	struct sbiret sbi_res;
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
