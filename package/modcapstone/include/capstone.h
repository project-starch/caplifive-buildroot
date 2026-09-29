#ifndef __CAPSTONE_H_
#define __CAPSTONE_H_

#define CAPSTONE_DEV_PATH "/dev/capstone"

// ioctl code
#define IOC_MAGIC '\xb8'


typedef unsigned long dom_id_t;
typedef unsigned long region_id_t;

struct ioctl_dom_create_args {
    // virtual region containing code
    void *code_begin;
    size_t code_len;
    size_t entry_offset;
    void *s_load_begin;
    size_t s_load_len;
    size_t s_entry_offset;
    size_t s_size;
    dom_id_t dom_id;
    /* The domain's DECLARED resource requirement, from .capstone_domreq in the image.
     * Both zero means the image declares nothing and the module keeps its historical
     * rule, so every domain built before this behaves exactly as it used to.
     *
     * APPENDED, never inserted. The module copies sizeof(its own struct) from user, so
     * a loader built with these fields and a module built without simply drops them.
     * That is what lets the loader land before the module does. */
    size_t domreq_data;      /* bytes dom_data must hold, all four parts together */
    size_t domreq_stack;     /* how much of that is stack; diagnostics only */
    /* The file-backed prefix of [code_begin, code_begin + code_len): what the module
     * copies. The rest of the image is .bss and is zero; the module zeroes it with the
     * block tail instead of copying zeros from user space. 0 < copy_len <= code_len. */
    size_t copy_len;
};

struct ioctl_dom_call_args {
    dom_id_t dom_id;
    unsigned long retval;
};

struct ioctl_region_create_args {
    size_t len;
    region_id_t region_id;
    size_t mmap_offset; /* the mmap offset of the new region */
};

struct ioctl_region_share_annotated_args {
    dom_id_t dom_id;
    region_id_t region_id;
    unsigned long annotation_perm;
    unsigned long annotation_rev;
    unsigned retval;
};

struct ioctl_region_share_args {
    dom_id_t dom_id;
    region_id_t region_id;
    unsigned retval;
};

struct ioctl_region_revoke_args {
    region_id_t region_id;
    unsigned retval;
};
struct ioctl_region_release_args {
    region_id_t region_id;
    unsigned retval; /* 0: revoked, popped and freed; 1: revoked, the slot kept; else refused, see dmesg */
};

struct ioctl_region_share_child_args {
    dom_id_t dom_id;
    region_id_t parent_id;
    unsigned long offset;
    unsigned long len;
    unsigned long annotation_perm;
    unsigned retval;
};

struct ioctl_region_query_args {
    region_id_t region_id;
    size_t mmap_offset;
    size_t len;
};

struct ioctl_dom_sched_args {
    dom_id_t dom_id;
    // TODO: more?
};

#define IOCTL_DOM_CREATE			_IOWR(IOC_MAGIC, 0, struct ioctl_dom_create_args)
#define IOCTL_DOM_CALL  			_IOWR(IOC_MAGIC, 1, struct ioctl_dom_call_args)
#define IOCTL_REGION_CREATE         _IOWR(IOC_MAGIC, 2, struct ioctl_region_create_args)
#define IOCTL_REGION_SHARE          _IOWR(IOC_MAGIC, 3, struct ioctl_region_share_args)
#define IOCTL_REGION_QUERY          _IOWR(IOC_MAGIC, 4, struct ioctl_region_query_args)
#define IOCTL_REGION_PROBE          _IO(IOC_MAGIC, 5)
#define IOCTL_DOM_SCHEDULE          _IOWR(IOC_MAGIC, 6, struct ioctl_dom_sched_args)
#define IOCTL_REGION_SHARE_ANNOTATED          _IOWR(IOC_MAGIC, 7, struct ioctl_region_share_annotated_args)
#define IOCTL_REGION_REVOKE          _IOWR(IOC_MAGIC, 8, struct ioctl_region_revoke_args)
#define IOCTL_REGION_SHARE_CHILD     _IOWR(IOC_MAGIC, 9, struct ioctl_region_share_child_args)
#define IOCTL_REGION_RELEASE         _IOWR(IOC_MAGIC, 10, struct ioctl_region_release_args)

/* A step always returns to Linux, including on preemption and domain fault.
 * ABI v1 is RV64-only; its separate event distinguishes a fault from an exit. */
struct ioctl_dom_step_args {
    unsigned long version;
    dom_id_t dom_id;
    unsigned long event;
    unsigned long result;
    unsigned long cause;
    unsigned long pc;
    unsigned long address;
};
#define CAPSTONE_STEP_RETURNED 0
#define CAPSTONE_STEP_PREEMPTED 1
#define CAPSTONE_STEP_FAULT 2
struct ioctl_process_stats {
    unsigned long version;
    unsigned long live_domains, live_regions, live_bytes;
    unsigned long cached_bytes, poisoned_blocks;
    unsigned long nodes_high_water, nodes_live, nodes_retired;
    unsigned long nodes_allocated_total, tag_pages, node_capacity;
};
#define IOCTL_PROCESS_STATS _IOR(IOC_MAGIC, 13, struct ioctl_process_stats)
#define IOCTL_PROCESS_ENABLE _IO(IOC_MAGIC, 12)
#define IOCTL_DOM_STEP _IOWR(IOC_MAGIC, 11, struct ioctl_dom_step_args)

#endif
