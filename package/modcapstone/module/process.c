/* Included by capstone.c: the managed interface shares its serialized monitor.
 *
 * Cached extents deliberately stay allocated: the monitor retains the original
 * physical authority, so Linux must not free/reallocate a range behind its back.
 * Best-fit reuse retains a pool shaped by prior working sets; a hard cache limit
 * reports ENOSPC instead of consuming the VM's CMA area indefinitely. Accounting
 * distinguishes live ownership from cached storage. A cache pins the module.
 */
#define PROCESS_DOMAINS 32
#define PROCESS_REGIONS MAX_REGION_N
#define PROCESS_MAX_BYTES (512UL * 1024 * 1024)

struct process_block {
    struct process_owner *owner;
    struct page *pages;
    dma_addr_t dma;
    size_t bytes;
    unsigned long id;
    bool poisoned;
    bool shared, transferred;
    atomic_t mappings;
};

static struct process_block process_domains[PROCESS_DOMAINS];
static struct process_block process_regions[PROCESS_REGIONS];
static unsigned long process_cached_bytes;
static unsigned long process_cache_limit = 384UL * 1024 * 1024;
module_param_named(process_cache_bytes, process_cache_limit, ulong, 0444);
MODULE_PARM_DESC(process_cache_bytes, "Maximum retained process storage in bytes");
static bool process_pinned;

static struct process_block *process_find(struct process_block *blocks, unsigned n,
                                          struct process_owner *owner, unsigned long id)
{
    unsigned i;
    for (i = 0; i < n; ++i)
        if (blocks[i].pages && blocks[i].owner == owner && blocks[i].id == id)
            return &blocks[i];
    return NULL;
}

static struct process_block *process_acquire(struct process_block *blocks,
        unsigned n, struct process_owner *owner, size_t bytes, bool *fresh)
{
    struct process_block *best = NULL, *empty = NULL;
    unsigned i;
    *fresh = false;
    for (i = 0; i < n; ++i) {
        struct process_block *b = &blocks[i];
        if (!b->pages) {
            if (!empty) empty = b;
        } else if (!b->owner && !b->poisoned && b->bytes >= bytes &&
                   (!best || b->bytes < best->bytes)) {
            best = b;
        }
    }
    if (best) {
        best->owner = owner;
        best->shared = best->transferred = false;
        return best;
    }
    if (!empty || bytes > process_cache_limit ||
        process_cached_bytes > process_cache_limit - bytes)
        return ERR_PTR(-ENOSPC);
    if (!region_dev)
        return ERR_PTR(-ENODEV);
    empty->pages = dma_alloc_pages(&region_dev->dev, bytes, &empty->dma,
                                   DMA_BIDIRECTIONAL, GFP_KERNEL);
    if (!empty->pages)
        return ERR_PTR(-ENOMEM);
    if (page_to_phys(empty->pages) % capstone_repr_granule(bytes)) {
        dma_free_pages(&region_dev->dev, bytes, empty->pages, empty->dma,
                       DMA_BIDIRECTIONAL);
        memset(empty, 0, sizeof(*empty));
        return ERR_PTR(-EINVAL);
    }
    atomic_set(&empty->mappings, 0);
    empty->bytes = bytes;
    empty->owner = owner;
    process_cached_bytes += bytes;
    *fresh = true;
    return empty;
}

/* Only a fresh allocation that the monitor never accepted may be freed. */
static void process_rollback(struct process_block *b, bool fresh)
{
    if (fresh) {
        dma_free_pages(&region_dev->dev, b->bytes, b->pages, b->dma,
                       DMA_BIDIRECTIONAL);
        process_cached_bytes -= b->bytes;
        memset(b, 0, sizeof(*b));
    } else {
        b->owner = NULL;
    }
}

static void process_pin(void)
{
    if (!process_pinned) {
        __module_get(THIS_MODULE);
        process_pinned = true;
    }
}

static long process_create_domain(struct process_owner *owner, void __user *arg)
{
    struct ioctl_dom_create_args a;
    struct process_block *b;
    struct sbiret r;
    unsigned long data, total, bytes;
    bool fresh;
    if (copy_from_user(&a, arg, sizeof(a))) return -EFAULT;
    if (a.s_size || a.s_load_len || !a.code_len || !a.copy_len || a.copy_len > a.code_len ||
        a.code_len > PROCESS_MAX_BYTES || a.domreq_data > PROCESS_MAX_BYTES ||
        a.domreq_stack > a.domreq_data ||
        (a.entry_offset & 0xffffffffUL) >= a.code_len ||
        (a.entry_offset >> 32) >= a.code_len)
        return -EINVAL;
    data = a.domreq_data ? a.domreq_data + MONITOR_SPLIT_SLACK :
                          max(a.code_len, (size_t)DOMAIN_DATA_SIZE);
    total = a.code_len + data;
    if (total > PROCESS_MAX_BYTES) return -E2BIG;
    bytes = roundup_pow_of_two(PAGE_ALIGN(total));
    b = process_acquire(process_domains, PROCESS_DOMAINS, owner, bytes, &fresh);
    if (IS_ERR(b)) return PTR_ERR(b);
    if (copy_from_user(page_address(b->pages), a.code_begin, a.copy_len)) {
        process_rollback(b, fresh);
        return -EFAULT;
    }
    /* Fresh pages hold whatever the allocator left; everything past the copied
     * prefix (.bss and the unused tail) must be zero. A cached block was filled
     * with zero capabilities by the monitor's reclaim when its last owner
     * released it (managed_reclaim), granule by granule, and the region path
     * already relies on that; zeroing it again here cost a launch a second per
     * 100 MB in the guest. */
    if (fresh)
        memset(page_address(b->pages) + a.copy_len, 0, b->bytes - a.copy_len);
    r = sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_DOM_CREATE,
                  page_to_phys(b->pages), a.code_len, b->bytes,
                  a.entry_offset, 0, 1);
    if (r.error || r.value < 0) {
        process_rollback(b, fresh);
        return -ENOSPC;
    }
    b->id = r.value;
    process_pin();
    a.dom_id = b->id;
    return copy_to_user(arg, &a, sizeof(a)) ? -EFAULT : 0;
}

static long process_create_region(struct process_owner *owner, void __user *arg)
{
    struct ioctl_region_create_args a;
    struct process_block *b;
    struct sbiret r;
    struct RegionInfo *region;
    size_t bytes;
    bool fresh;
    if (copy_from_user(&a, arg, sizeof(a))) return -EFAULT;
    if (!a.len || a.len > PROCESS_MAX_BYTES) return -EINVAL;
    bytes = roundup_pow_of_two(PAGE_ALIGN(a.len));
    b = process_acquire(process_regions, PROCESS_REGIONS, owner, bytes, &fresh);
    if (IS_ERR(b)) return PTR_ERR(b);
    if (fresh) {
        memset(page_address(b->pages), 0, b->bytes);
        r = sbi_ecall(SBI_EXT_CAPSTONE, SBI_CAPSTONE_PROCESS_REGION_CREATE, page_to_phys(b->pages),
                      b->bytes, 0, 0, 0, 0);
    } else {
        r = sbi_ecall(SBI_EXT_CAPSTONE, SBI_CAPSTONE_PROCESS_REGION_PREPARE, b->id, 0, 0, 0, 0, 0);
    }
    if (r.error || r.value < 0) {
        process_rollback(b, fresh);
        return -ENOSPC;
    }
    if (fresh) b->id = r.value;
    process_pin();
    probe_regions();
    if (b->id >= MAX_REGION_N) {
        b->poisoned = true;
        return -EIO;
    }
    region = &regions[b->id];
    a.region_id = b->id;
    a.len = b->bytes;
    a.mmap_offset = region->mmap_offset;
    return copy_to_user(arg, &a, sizeof(a)) ? -EFAULT : 0;
}

static void process_release(struct process_owner *owner)
{
    unsigned group, i;
    if (!owner->managed) return;
    /* Quiesce and revoke executable state before revoking its grants. No ioctl
     * is in flight after the last struct file reference reaches release(). */
    for (group = 0; group < 2; ++group) {
        struct process_block *blocks = group ? process_regions : process_domains;
        unsigned n = group ? PROCESS_REGIONS : PROCESS_DOMAINS;
        for (i = 0; i < n; ++i) {
            struct process_block *b = &blocks[i];
            struct sbiret r;
            if (b->owner != owner) continue;
            r = sbi_ecall(SBI_EXT_CAPSTONE, group ? SBI_CAPSTONE_PROCESS_REGION_RESET : SBI_CAPSTONE_PROCESS_DESTROY,
                          b->id, 0, 0, 0, 0, 0);
            if (r.error || r.value) {
                b->poisoned = true;
                pr_err("capstone: process extent %lu could not be reclaimed\n", b->id);
            }
            b->owner = NULL;
        }
    }
    sbi_ecall(SBI_EXT_CAPSTONE, SBI_CAPSTONE_PROCESS_COLLECT, 0, 0, 0, 0, 0, 0);
}

static void process_vma_open(struct vm_area_struct *vma)
{
    struct process_block *b = vma->vm_private_data;
    atomic_inc(&b->mappings);
}

static void process_vma_close(struct vm_area_struct *vma)
{
    struct process_block *b = vma->vm_private_data;
    atomic_dec(&b->mappings);
}

static const struct vm_operations_struct process_vm_ops = {
    .open = process_vma_open,
    .close = process_vma_close,
};

static int process_mmap(struct process_owner *owner, struct vm_area_struct *vma)
{
    size_t bytes = vma->vm_end - vma->vm_start;
    unsigned i;
    int result = -EINVAL;
    if (mutex_lock_interruptible(&capstone_lock)) return -EINTR;
    for (i = 0; i < PROCESS_REGIONS; ++i) {
        struct process_block *b = &process_regions[i];
        if (b->owner != owner || !b->pages || b->transferred) continue;
        if (vma->vm_pgoff != regions[b->id].mmap_offset >> PAGE_SHIFT ||
            !bytes || bytes > b->bytes) continue;
        vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP;
        result = remap_pfn_range(vma, vma->vm_start, page_to_pfn(b->pages),
                                 bytes, vma->vm_page_prot);
        if (!result) {
            vma->vm_ops = &process_vm_ops;
            vma->vm_private_data = b;
            process_vma_open(vma);
        }
        break;
    }
    mutex_unlock(&capstone_lock);
    return result;
}

static long process_stats(void __user *arg)
{
    struct ioctl_process_stats stats = {.version = 1};
    unsigned group, i;
    unsigned long *nodes[] = {&stats.nodes_high_water, &stats.nodes_live,
        &stats.nodes_retired, &stats.nodes_allocated_total, &stats.tag_pages,
        &stats.node_capacity};
    stats.cached_bytes = process_cached_bytes;
    for (group = 0; group < 2; ++group) {
        struct process_block *blocks = group ? process_regions : process_domains;
        unsigned n = group ? PROCESS_REGIONS : PROCESS_DOMAINS;
        for (i = 0; i < n; ++i) {
            struct process_block *b = &blocks[i];
            if (b->poisoned) ++stats.poisoned_blocks;
            if (!b->owner) continue;
            if (group) ++stats.live_regions;
            else ++stats.live_domains;
            stats.live_bytes += b->bytes;
        }
    }
    for (i = 0; i < 6; ++i) {
        struct sbiret r = sbi_ecall(SBI_EXT_CAPSTONE, SBI_CAPSTONE_PROCESS_STATS, i + 1, 0, 0, 0, 0, 0);
        if (r.error) return -EIO;
        *nodes[i] = r.value;
    }
    return copy_to_user(arg, &stats, sizeof(stats)) ? -EFAULT : 0;
}

static long process_ioctl(struct process_owner *owner, unsigned number, void __user *arg)
{
    struct ioctl_dom_step_args step;
    struct ioctl_region_share_annotated_args share;
    struct ioctl_region_query_args query;
    struct process_block *b;
    struct sbiret r;
    switch (number) {
    case IOCTL_PROCESS_STATS:
        return process_stats(arg);
    case IOCTL_DOM_CREATE:
        return process_create_domain(owner, arg);
    case IOCTL_REGION_CREATE:
        return process_create_region(owner, arg);
    case IOCTL_DOM_STEP:
        if (copy_from_user(&step, arg, sizeof(step))) return -EFAULT;
        if (!process_find(process_domains, PROCESS_DOMAINS, owner, step.dom_id))
            return -EPERM;
        return ioctl_step_dom(arg);
    case IOCTL_REGION_SHARE_ANNOTATED:
        if (copy_from_user(&share, arg, sizeof(share))) return -EFAULT;
        if (!process_find(process_domains, PROCESS_DOMAINS, owner, share.dom_id) ||
            !process_find(process_regions, PROCESS_REGIONS, owner, share.region_id))
            return -EPERM;
        if (share.annotation_perm > 4 ||
            (share.annotation_rev != 2 && share.annotation_rev != 3)) return -EINVAL;
        b = process_find(process_regions, PROCESS_REGIONS, owner, share.region_id);
        if (b->shared || (share.annotation_rev == 3 && atomic_read(&b->mappings)))
            return -EBUSY;
        /* Transfer removes Linux authority. Refuse it while a Linux VMA exists,
         * including a forked VMA, and refuse future mappings until reclamation. */
        b->shared = true;
        b->transferred = share.annotation_rev == 3;
        r = sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_SHARE_ANNOTATED,
                      share.dom_id, share.region_id, share.annotation_perm,
                      share.annotation_rev, 0, 0);
        while (!r.error && r.value == CAPSTONE_STEP_PREEMPTED) {
            if (signal_pending(current)) return -EINTR;
            cond_resched();
            r = sbi_ecall(SBI_EXT_CAPSTONE, SBI_CAPSTONE_PROCESS_RESUME_SHARE, share.dom_id, 0, 0, 0, 0, 0);
        }
        if (r.value == CAPSTONE_STEP_FAULT) return -EFAULT;
        if (r.error || r.value) return -EIO;
        share.retval = 0;
        return copy_to_user(arg, &share, sizeof(share)) ? -EFAULT : 0;
    case IOCTL_REGION_PROBE:
        return 0;
    case IOCTL_REGION_QUERY:
        if (copy_from_user(&query, arg, sizeof(query))) return -EFAULT;
        b = process_find(process_regions, PROCESS_REGIONS, owner, query.region_id);
        query.len = b ? b->bytes : 0;
        query.mmap_offset = b ? regions[b->id].mmap_offset : 0;
        return copy_to_user(arg, &query, sizeof(query)) ? -EFAULT : 0;
    default:
        return -ENOTTY;
    }
}
