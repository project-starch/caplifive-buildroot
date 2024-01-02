#ifndef __CAPSTONE_H_
#define __CAPSTONE_H_

#define CAPSTONE_DEV_PATH "/dev/capstone"

// ioctl code
#define IOC_MAGIC '\xb8'


typedef unsigned long dom_id_t;

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

struct ioctl_dom_get_data_args {
    dom_id_t dom_id;
    unsigned long retval;
};

#define IOCTL_DOM_CREATE			_IOWR(IOC_MAGIC, 0, struct ioctl_dom_create_args)
#define IOCTL_DOM_CALL  			_IOWR(IOC_MAGIC, 1, struct ioctl_dom_call_args)
#define IOCTL_REGION_CREATE         _IOWR(IOC_MAGIC, 2, struct ioctl_region_create_args)
#define IOCTL_REGION_SHARE          _IOWR(IOC_MAGIC, 3, struct ioctl_region_share_args)
#define IOCTL_DOM_DATA_GET          _IOWR(IOC_MAGIC, 2, struct ioctl_dom_get_data_args)

#endif
