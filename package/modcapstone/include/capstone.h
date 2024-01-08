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
    // TODO: pass info about the required data region size
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

struct ioctl_region_share_args {
    dom_id_t dom_id;
    region_id_t region_id;
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

#endif
