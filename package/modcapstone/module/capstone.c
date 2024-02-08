#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/printk.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/miscdevice.h>
#include <asm/sbi.h>
#include <asm/errno.h>
#include <asm/string.h>
#include "../include/capstone.h"
#include "capstone-sbi.h"

#define DEVICE_NAME "capstone"
#define DEVICE_FILE_NAME "capstone"

#define DOMAIN_DATA_SIZE (4096 * 16)
#define MAP_SIZE_LIMIT 0x10000000

#define SUCCESS 0


#define MAX_REGION_N 64

struct RegionInfo {
	region_id_t region_id; /* TODO: it is now assumed that region_id is the same as the index in regions[] */
	unsigned long base_paddr;
	size_t len;
	size_t mmap_offset;
};

static size_t pre_mmap_offset;
static struct RegionInfo regions[MAX_REGION_N];
static int region_n;

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

static void ioctl_create_dom(struct ioctl_dom_create_args* __user args) {
	struct ioctl_dom_create_args m_args;

	copy_from_user(&m_args, args, sizeof(struct ioctl_dom_create_args));

	if(m_args.s_size < m_args.s_load_len) {
		pr_alert("Invalid arguments for ioctl_create_dom: s_size must not be lower than s_load_len\n");
		return;
	}

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
	pr_info("code size = %lu, tot_size = %lx, entry_offset = %lx\n", m_args.code_len, (1 << dom_pages_log2) * PAGE_SIZE, m_args.entry_offset);

	copy_from_user((void*)dom_vaddr, m_args.code_begin, m_args.code_len);

	struct sbiret sbi_res = sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_DOM_CREATE,
		/* base paddr = */ dom_paddr,
		/* code size = */ m_args.code_len,
		/* tot size = */ (1 << dom_pages_log2) * PAGE_SIZE, 
		/* entry offset = */ m_args.entry_offset,
		0, 0);
	m_args.dom_id = (dom_id_t)sbi_res.value;
	copy_to_user(args, &m_args, sizeof(struct ioctl_dom_create_args));

	if (m_args.s_load_len > 0) {
		// this domain has an S mode
		// call the domain with the code region to allow initialisation

		unsigned long dom_s_load_pages = (m_args.s_size - 1) / PAGE_SIZE + 1;
		unsigned long dom_s_load_pages_log2 = dom_s_load_pages == 1 ? 0 : (ilog2(dom_s_load_pages - 1) + 1);
		unsigned long dom_s_load_vaddr = (unsigned long)__get_free_pages(GFP_HIGHUSER, dom_s_load_pages_log2);
		if(!dom_s_load_pages) {
			pr_alert("Failed to allocate S-mode code region for domain.\n");
			return;
		}

		pr_info("Domain S-mode region vaddr = %lx, paddr = %lx\n", dom_s_load_vaddr, __pa(dom_s_load_vaddr));

		copy_from_user((void*)dom_s_load_vaddr, m_args.s_load_begin, m_args.s_load_len);
		memset((void*)(dom_s_load_vaddr + m_args.s_load_len), 0, m_args.s_size - m_args.s_load_len);

		sbi_res = sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_DOM_CALL_WITH_CAP,
			m_args.dom_id, __pa(dom_s_load_vaddr), m_args.s_size,
			__pa(dom_s_load_vaddr) + m_args.s_entry_offset, 0, 0);

		if (sbi_res.value) {
			pr_alert("Failed to initialise S mode\n");
		}
	}
}

unsigned long call_dom(dom_id_t dom_id)  {
	struct sbiret sbi_res = sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_DOM_CALL,
				dom_id, 0, 0, 0, 0, 0);
	return sbi_res.value;
}
EXPORT_SYMBOL(call_dom);

static void ioctl_call_dom(struct ioctl_dom_call_args* __user args) {
	struct ioctl_dom_call_args m_args;
	copy_from_user(&m_args, args, sizeof(struct ioctl_dom_call_args));

	m_args.retval = call_dom(m_args.dom_id);

	copy_to_user(args, &m_args, sizeof(struct ioctl_dom_call_args));
}

static void probe_regions(void);

static void ioctl_create_region(struct ioctl_region_create_args* __user args) {
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

	if(region_n > m_args.region_id) {
		pr_alert("Region ID reuse detected.\n");
	} else if(region_n != m_args.region_id) {
		probe_regions();
		if(region_n <= m_args.region_id) {
			pr_alert("Failed to fetch information about the newly created region.\n");
		}
	} else {
		regions[region_n].region_id = m_args.region_id;
		regions[region_n].base_paddr = __pa(vaddr);
		regions[region_n].len = m_args.len;
		regions[region_n].mmap_offset = pre_mmap_offset;
		/* We need to round up to page size due to the limitation of mmap */
		if(m_args.len < MAP_SIZE_LIMIT) {
			pre_mmap_offset = round_up(pre_mmap_offset + regions[region_n].len, PAGE_SIZE);
		}
		++ region_n;
	}

	copy_to_user(args, &m_args, sizeof(struct ioctl_region_create_args));
}

static void ioctl_revoke_region(struct ioctl_region_revoke_args* __user args) {
	struct ioctl_region_revoke_args m_args;
	copy_from_user(&m_args, args, sizeof(struct ioctl_region_revoke_args));

	struct sbiret sbi_res = sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_REVOKE,
				m_args.region_id, 0, 0, 0, 0, 0);
	m_args.retval = sbi_res.value;

	copy_to_user(args, &m_args, sizeof(struct ioctl_region_revoke_args));
}

static void ioctl_share_region_annotated(struct ioctl_region_share_annotated_args* __user args) {
	struct ioctl_region_share_annotated_args m_args;
	copy_from_user(&m_args, args, sizeof(struct ioctl_region_share_annotated_args));

	struct sbiret sbi_res = sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_SHARE_ANNOTATED,
				m_args.dom_id, m_args.region_id, m_args.annotation_perm, m_args.annotation_rev, 0, 0);
	m_args.retval = sbi_res.value;

	copy_to_user(args, &m_args, sizeof(struct ioctl_region_share_annotated_args));
}

unsigned long share_region(dom_id_t dom_id, region_id_t region_id) {
	struct sbiret sbi_res = sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_SHARE,
				dom_id, region_id, 0, 0, 0, 0);
	return sbi_res.value;	
}
EXPORT_SYMBOL(share_region);

static void ioctl_share_region(struct ioctl_region_share_args* __user args) {
	struct ioctl_region_share_args m_args;
	copy_from_user(&m_args, args, sizeof(struct ioctl_region_share_args));

	m_args.retval = share_region(m_args.dom_id, m_args.region_id);

	copy_to_user(args, &m_args, sizeof(struct ioctl_region_share_args));
}

static void probe_regions(void) {
	struct sbiret sbi_res = sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_COUNT,
			0, 0, 0, 0, 0, 0);
	int new_region_n = sbi_res.value;
	while(region_n < new_region_n) {
		/* query information about the region */
		regions[region_n].region_id = region_n;
		sbi_res = sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_QUERY,
			region_n, CAPSTONE_REGION_FIELD_BASE, 0, 0, 0, 0);
		regions[region_n].base_paddr = sbi_res.value;
		sbi_res = sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_QUERY,
			region_n, CAPSTONE_REGION_FIELD_LEN, 0, 0, 0, 0);
		regions[region_n].len = sbi_res.value;
		regions[region_n].mmap_offset = pre_mmap_offset;
		if(regions[region_n].len < MAP_SIZE_LIMIT) {
			pre_mmap_offset = round_up(pre_mmap_offset + regions[region_n].len, PAGE_SIZE);
		}
		++ region_n;
	}
	region_n = new_region_n;
}

static void ioctl_region_query(struct ioctl_region_query_args* __user args) {
	struct ioctl_region_query_args m_args;
	copy_from_user(&m_args, args, sizeof(struct ioctl_region_query_args));
	if(m_args.region_id >= region_n) {
		m_args.len = m_args.mmap_offset = 0;
	} else {
		if(regions[m_args.region_id].region_id != m_args.region_id) {
			pr_info("Region ID different from the index of the region!\n");
		}
		m_args.len = regions[m_args.region_id].len;
		m_args.mmap_offset = regions[m_args.region_id].mmap_offset;
	}

	copy_to_user(args, &m_args, sizeof(struct ioctl_region_query_args));
}

static void ioctl_schedule_dom(struct ioctl_dom_sched_args* __user args) {
	struct ioctl_dom_sched_args m_args;
	copy_from_user(&m_args, args, sizeof(struct ioctl_dom_sched_args));

	struct sbiret sbi_res = sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_DOM_SCHEDULE,
		m_args.dom_id, 0, 0, 0, 0, 0);
}

static long device_ioctl(struct file* file,
					     unsigned int ioctl_num,
						 unsigned long ioctl_param)
{
	switch (ioctl_num) {
		case IOCTL_DOM_CREATE:
			ioctl_create_dom((struct ioctl_dom_create_args* __user)ioctl_param);
			break;
		case IOCTL_DOM_CALL:
			ioctl_call_dom((struct ioctl_dom_call_args* __user)ioctl_param);
			break;
		case IOCTL_REGION_CREATE:
		 	ioctl_create_region((struct ioctl_region_create_args* __user)ioctl_param);
			break;
		case IOCTL_REGION_SHARE:
		 	ioctl_share_region((struct ioctl_region_share_args* __user)ioctl_param);
			break;
		case IOCTL_REGION_QUERY:
			ioctl_region_query((struct ioctl_region_query_args* __user)ioctl_param);
			break;
		case IOCTL_REGION_PROBE:
			probe_regions();
			break;
		case IOCTL_DOM_SCHEDULE:
			ioctl_schedule_dom((struct ioctl_dom_sched_args* __user)ioctl_param);
			break;
		case IOCTL_REGION_SHARE_ANNOTATED:
			ioctl_share_region_annotated((struct ioctl_region_share_annotated_args* __user)ioctl_param);
			break;
		case IOCTL_REGION_REVOKE:
			ioctl_revoke_region((struct ioctl_region_revoke_args* __user)ioctl_param);
			break;
		default:
			pr_info("Unrecognised IOCTL command %u\n", ioctl_num);
	}
	return 0;
}

static int device_mmap(struct file *filp, struct vm_area_struct *vma) {
	if(!region_n)
		return -EINVAL;
	int i;
	size_t vm_offset = vma->vm_pgoff << PAGE_SHIFT;
	size_t vm_size = vma->vm_end - vma->vm_start;
	for(i = 0; i < region_n; i ++) {
		pr_info("mmap[%d]: %lx %lx", regions[i].region_id, regions[i].base_paddr, regions[i].base_paddr + regions[i].len);
	}
	for(i = 0; i < region_n && 
		!(regions[i].len < MAP_SIZE_LIMIT && vm_offset >= regions[i].mmap_offset && vm_offset + vm_size <= regions[i].mmap_offset + regions[i].len);
		i ++);
	if(i >= region_n)
		return -EINVAL;
	struct RegionInfo *region_info = &regions[i];

	remap_pfn_range(vma, vma->vm_start,
		(region_info->base_paddr + (vm_offset - region_info->mmap_offset)) >> PAGE_SHIFT,
		vm_size,
		vma->vm_page_prot);

	return 0;
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.read = device_read,
	.write = device_write,
	.unlocked_ioctl = device_ioctl,
	.open = device_open,
	.mmap = device_mmap,
	.release = device_release
};

static struct miscdevice capstone_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "capstone",
	.fops = &fops,
	.mode = 0666	
};

static int __init capstone_init(void)
{
	int retval = misc_register(&capstone_dev);
	if (retval < 0) {
		pr_alert("Failed to register device\n");
		return retval;
	}

	region_n = 0;
	pre_mmap_offset = 0;

	return 0;
}

static void __exit capstone_exit(void)
{
	misc_deregister(&capstone_dev);
}


module_init(capstone_init);
module_exit(capstone_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Captainer kernel module");
