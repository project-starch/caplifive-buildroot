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
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
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


/* M-2 (ISSUES.md; monitor-unification.md Phase B item 9, 2026-09-08): this mirror of the monitor's
   region table must hold every slot the monitor can hand out -- CAPSTONE_MAX_REGION_N is 96 on both
   targets since Phase B item 3 -- AND the copy below is bounded, because the previous 64-entry array
   was filled past its end by probe_regions() with no check (silently: the overrun is read back
   through the same out-of-bounds index, so no ioctl ever saw it). Overridable with -DMAX_REGION_N
   only for the bound's positive control (built at 64, the module must REFUSE ids >= 64 and warn). */
#ifndef MAX_REGION_N
#define MAX_REGION_N 96
#endif

struct RegionInfo {
	region_id_t region_id; /* TODO: it is now assumed that region_id is the same as the index in regions[] */
	unsigned long base_paddr;
	size_t len;
	size_t mmap_offset;
	struct page *pages; /* the memory, when this module allocated it; NULL for a region mirrored from the monitor */
};

/*
 * A region is one physically contiguous block: the domain addresses it through one
 * capability, without page tables. Below the buddy allocator's largest block (4 MiB,
 * MAX_ORDER 11) __get_free_pages served; a pool for a database benchmark wants a
 * hundred times that. dma_alloc_pages() takes such a block from the CMA area the
 * kernel reserved at boot (cma= on the command line, a reserved-memory node in the
 * device tree) and falls back to the buddy allocator below its limit, so every region
 * takes the one path. The pages keep their ordinary cached kernel mapping, as before;
 * the device exists only to carry the DMA mask the allocator asks for.
 */
static struct platform_device *region_dev;

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
	/* Headroom must SCALE with the image, not be a fixed 64 KiB.
	 *
	 * dom_tot_size is rounded UP to a power-of-two page count, and everything the domain
	 * owns at run time -- globals blob, cap table, heap and STACK -- comes out of whatever
	 * is left after code_size. For a 10 KB ladder rung, 64 KiB of headroom rounds to
	 * 128 KiB and is plenty. For SQLite at ~1.38 MB of code it rounds to 2 MiB, leaving
	 * ~700 KB for all four -- so the heap and the stack have to be traded against each
	 * other, and at 512 KiB of heap the stack is down to 57 KiB, which a recursive
	 * expression walker overruns on its own. Doubling for large images pushes SQLite to
	 * order-10 (4 MiB), which leaves ~2.6 MB of dom_data: a 1 MiB heap and a ~1.5 MB stack
	 * at the same time, so neither is sized by the other.
	 *
	 * WHAT THIS DOES NOT CLAIM. An earlier version of this comment said memory starvation
	 * was the CAUSE of the SQLite wedge. That is RETRACTED (2026-08-12): the domain was
	 * subsequently run with 4x the heap and 5x the stack and wedged on the IDENTICAL
	 * instruction, so the size was never the fault -- the real cause was data corruption
	 * (S-06). This change stands anyway, because trading heap against stack made every
	 * later experiment ambiguous: a failure could always be blamed on whichever of the two
	 * had been shrunk. Sizing both generously removes that confound; it fixes nothing.
	 *
	 * Written as max(code_len, DOMAIN_DATA_SIZE) so SMALL DOMAINS ARE UNCHANGED: any image
	 * below 64 KiB keeps exactly the headroom, page count and order it had before, so every
	 * existing ladder rung's geometry -- and the published numbers taken with it -- is
	 * byte-identical. Only images larger than 64 KiB see any difference at all. */
	unsigned long dom_headroom = m_args.code_len > DOMAIN_DATA_SIZE
	                                 ? m_args.code_len : DOMAIN_DATA_SIZE;
	unsigned long dom_tot_size = m_args.code_len + dom_headroom;
	unsigned long dom_pages = (dom_tot_size - 1) / PAGE_SIZE + 1;
	unsigned long dom_pages_log2 = dom_pages == 1 ? 0 : (ilog2(dom_pages - 1) + 1);

	unsigned long dom_vaddr = (unsigned long)__get_free_pages(GFP_HIGHUSER | __GFP_ZERO, dom_pages_log2);
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
	if (sbi_res.error) {
		pr_err("DOM_CREATE failed: paddr=%lx code_len=%lu tot_size=%lx entry_offset=%lx error=%ld value=%ld\n",
			dom_paddr, m_args.code_len, (1 << dom_pages_log2) * PAGE_SIZE,
			m_args.entry_offset, sbi_res.error, sbi_res.value);
		m_args.dom_id = (dom_id_t)-1;
		copy_to_user(args, &m_args, sizeof(struct ioctl_dom_create_args));
		return;
	}
	m_args.dom_id = (dom_id_t)sbi_res.value;
	copy_to_user(args, &m_args, sizeof(struct ioctl_dom_create_args));

	if (m_args.s_load_len > 0) {
		// this domain has an S mode
		// call the domain with the code region to allow initialisation

		unsigned long dom_s_load_pages = (m_args.s_size - 1) / PAGE_SIZE + 1;
		unsigned long dom_s_load_pages_log2 = dom_s_load_pages == 1 ? 0 : (ilog2(dom_s_load_pages - 1) + 1);
		unsigned long dom_s_load_actual_size = (1 << dom_s_load_pages_log2) * PAGE_SIZE;
		unsigned long dom_s_load_vaddr = (unsigned long)__get_free_pages(GFP_HIGHUSER | __GFP_ZERO, dom_s_load_pages_log2);
		if(!dom_s_load_pages) {
			pr_alert("Failed to allocate S-mode code region for domain.\n");
			return;
		}

		pr_info("Domain S-mode region vaddr = %lx, paddr = %lx\n", dom_s_load_vaddr, __pa(dom_s_load_vaddr));

		copy_from_user((void*)dom_s_load_vaddr, m_args.s_load_begin, m_args.s_load_len);
		memset((void*)(dom_s_load_vaddr + m_args.s_load_len), 0, dom_s_load_actual_size - m_args.s_load_len);

		sbi_res = sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_DOM_CALL_WITH_CAP,
			m_args.dom_id, __pa(dom_s_load_vaddr), dom_s_load_actual_size,
			__pa(dom_s_load_vaddr) + m_args.s_entry_offset, 0, 0);
		if (sbi_res.error || sbi_res.value) {
			pr_err("DOM_CALL_WITH_CAP failed: dom_id=%lu s_paddr=%lx s_size=%lu s_entry=%lx error=%ld value=%ld\n",
				m_args.dom_id, __pa(dom_s_load_vaddr), dom_s_load_actual_size,
				__pa(dom_s_load_vaddr) + m_args.s_entry_offset, sbi_res.error,
				sbi_res.value);
		}

		if (sbi_res.value) {
			pr_alert("Failed to initialise S mode\n");
		} else {
			pr_info("S mode initialisation successful\n");
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

	size_t size = PAGE_ALIGN(m_args.len);
	dma_addr_t dma;
	struct page *pages;
	unsigned long paddr;

	if (!region_dev) {
		pr_alert("capstone: no region device, cannot allocate a region\n");
		m_args.region_id = (region_id_t)-1;
		copy_to_user(args, &m_args, sizeof(struct ioctl_region_create_args));
		return;
	}
	/* zeroed, physically contiguous, cached: CMA above the buddy limit, buddy below it */
	pages = dma_alloc_pages(&region_dev->dev, size, &dma, DMA_BIDIRECTIONAL, GFP_KERNEL);
	if(!pages) {
		pr_alert("Failed to allocate memory region of %zu bytes (above 4 MiB it needs a CMA area: cma= on the kernel command line).\n", size);
		/* REPORT THE FAILURE. Returning without copy_to_user leaves the caller's own
		 * pre-initialised .region_id = -1 in ITS buffer and tells it nothing -- so
		 * create_region() hands back ULONG_MAX silently, map_region() then walks ids
		 * upward and returns NULL, and the failure surfaces as "map_region failed".
		 * That mislabelled the 64 MiB measurement in three committed documents: it was
		 * always a CREATE failure at the buddy allocator's order-10 wall, never a map
		 * one. The sbi_res.error path below already did this correctly; these two did
		 * not. */
		m_args.region_id = (region_id_t)-1;
		copy_to_user(args, &m_args, sizeof(struct ioctl_region_create_args));
		return;
	}
	paddr = page_to_phys(pages);

	struct sbiret sbi_res = sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_CREATE,
				paddr, m_args.len, 0, 0, 0, 0);
	if (sbi_res.error) {
		dma_free_pages(&region_dev->dev, size, pages, dma, DMA_BIDIRECTIONAL);
		pr_err("REGION_CREATE failed: len=%lu paddr=%lx error=%ld value=%ld\n",
			m_args.len, paddr, sbi_res.error, sbi_res.value);
		m_args.region_id = (region_id_t)-1;
		copy_to_user(args, &m_args, sizeof(struct ioctl_region_create_args));
		return;
	}
	m_args.region_id = sbi_res.value;

	if(region_n > m_args.region_id) {
		pr_alert("Region ID reuse detected.\n");
	} else if(region_n != m_args.region_id) {
		/* the monitor took a slot for the remainder it split off, so the new region's
		   id is past ours: mirror the slots between, then remember our pages */
		probe_regions();
		if(region_n <= m_args.region_id) {
			pr_alert("Failed to fetch information about the newly created region.\n");
		} else if (m_args.region_id < MAX_REGION_N) {
			regions[m_args.region_id].pages = pages;
		}
	} else if(region_n >= MAX_REGION_N) {
		pr_warn_once("capstone: region %lu not mirrored, module table full at %d (M-2)\n",
			(unsigned long)m_args.region_id, MAX_REGION_N);
	} else {
		regions[region_n].region_id = m_args.region_id;
		regions[region_n].base_paddr = paddr;
		regions[region_n].len = m_args.len;
		regions[region_n].mmap_offset = pre_mmap_offset;
		regions[region_n].pages = pages;
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

/*
 * Give a region back: the monitor revokes what the domain derived from it and pops
 * its slot, then the memory returns to the kernel. Only the newest region can go,
 * because the monitor's table is a stack; a caller that made a pool and its tables
 * releases them in the reverse order. A region the monitor gave away outright
 * (REV_TRANSFERRED) has no handle to revoke and stays, as it did before; a region
 * shared REV_BORROWED or REV_SHARED comes back whole.
 */
static void ioctl_release_region(struct ioctl_region_release_args* __user args) {
	struct ioctl_region_release_args m_args;
	struct sbiret sbi_res;
	struct RegionInfo *r;

	copy_from_user(&m_args, args, sizeof(struct ioctl_region_release_args));
	m_args.retval = (unsigned)-1;
	if (m_args.region_id >= region_n || m_args.region_id >= MAX_REGION_N) {
		pr_warn("capstone: release refused, region %lu is not mirrored here\n",
			(unsigned long)m_args.region_id);
		goto out;
	}
	r = &regions[m_args.region_id];
	if (!r->pages) {
		pr_warn("capstone: release refused, region %lu was not allocated here\n",
			(unsigned long)m_args.region_id);
		goto out;
	}
	/* the domain's access ends here, whatever happens to the slot */
	sbi_res = sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_REVOKE,
			m_args.region_id, 0, 0, 0, 0, 0);
	if (sbi_res.value != 0) {
		pr_warn("capstone: release refused, the monitor cannot revoke region %lu\n",
			(unsigned long)m_args.region_id);
		goto out;
	}
	/* the memory goes back only with the monitor's slot, and the monitor's table is a
	   stack: a region below a remainder slot the monitor split off stays, revoked, until
	   the slots above it are gone. Refusing to free here is what keeps one owner per page. */
	if (m_args.region_id != region_n - 1) {
		pr_info("capstone: region %lu revoked, kept: %d slot(s) above it\n",
			(unsigned long)m_args.region_id, region_n - 1 - (int)m_args.region_id);
		m_args.retval = 1;
		goto out;
	}
	sbi_res = sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_POP, 1, 0, 0, 0, 0, 0);
	if (sbi_res.value != 0) {
		pr_warn("capstone: the monitor did not pop region %lu after its revoke\n",
			(unsigned long)m_args.region_id);
		m_args.retval = 1;
		goto out;
	}
	dma_free_pages(&region_dev->dev, PAGE_ALIGN(r->len), r->pages,
		       (dma_addr_t)r->base_paddr, DMA_BIDIRECTIONAL);
	r->pages = NULL;
	/* Hand the offset window back too. Only the newest region reaches here, so its
	   window is the top one and restoring it undoes exactly what creating it did: a
	   region under MAP_SIZE_LIMIT advanced pre_mmap_offset past its own start, one at
	   or above never advanced it at all and this assignment is then a no-op. Without
	   it the offset space leaks monotonically across create/release cycles. */
	pre_mmap_offset = r->mmap_offset;
	r->len = 0;
	region_n--;
	m_args.retval = 0;
out:
	copy_to_user(args, &m_args, sizeof(struct ioctl_region_release_args));
}

static void ioctl_share_child_region(struct ioctl_region_share_child_args* __user args) {
	struct ioctl_region_share_child_args m_args;
	copy_from_user(&m_args, args, sizeof(struct ioctl_region_share_child_args));

	struct sbiret sbi_res = sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_SHARE_CHILD,
				m_args.dom_id, m_args.parent_id, m_args.offset, m_args.len,
				m_args.annotation_perm, 0);
	m_args.retval = sbi_res.value;

	copy_to_user(args, &m_args, sizeof(struct ioctl_region_share_child_args));
}

static void ioctl_share_region_annotated(struct ioctl_region_share_annotated_args* __user args) {
	struct ioctl_region_share_annotated_args m_args;
	/* Zero first: copy_from_user's return is not checked here, and a short copy must not leave
	   stack garbage in the fields the monitor reads. (The board's diagnostic print that came
	   with this memset is retired; the memset stays.) */
	memset(&m_args, 0, sizeof(m_args));
	copy_from_user(&m_args, args, sizeof(struct ioctl_region_share_annotated_args));

	struct sbiret sbi_res = sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_SHARE_ANNOTATED,
				m_args.dom_id, m_args.region_id, m_args.annotation_perm, m_args.annotation_rev, 0, 0);
	m_args.retval = sbi_res.value;

	copy_to_user(args, &m_args, sizeof(struct ioctl_region_share_annotated_args));
}

unsigned long share_region(dom_id_t dom_id, region_id_t region_id) {
	pr_info("share_region: %d\n", region_id);
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
	if(new_region_n > MAX_REGION_N) {
		pr_warn_once("capstone: monitor reports %d regions, this module tracks at most %d; ids >= %d are not mirrored (M-2)\n",
			new_region_n, MAX_REGION_N, MAX_REGION_N);
		new_region_n = MAX_REGION_N;
	}
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
		case IOCTL_REGION_SHARE_CHILD:
			ioctl_share_child_region((struct ioctl_region_share_child_args* __user)ioctl_param);
			break;
		case IOCTL_REGION_RELEASE:
			ioctl_release_region((struct ioctl_region_release_args* __user)ioctl_param);
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
	region_dev = platform_device_register_simple("capstone-regions", -1, NULL, 0);
	if (IS_ERR(region_dev)) {
		pr_alert("capstone: no region device (%ld)\n", PTR_ERR(region_dev));
		region_dev = NULL;
	} else if (dma_coerce_mask_and_coherent(&region_dev->dev, DMA_BIT_MASK(64))) {
		pr_alert("capstone: the region device takes no 64-bit DMA mask\n");
	}

	region_n = 0;
	pre_mmap_offset = 0;

	return 0;
}

static void __exit capstone_exit(void)
{
	if (region_dev)
		platform_device_unregister(region_dev);
	misc_deregister(&capstone_dev);
}


module_init(capstone_init);
module_exit(capstone_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Captainer kernel module");
