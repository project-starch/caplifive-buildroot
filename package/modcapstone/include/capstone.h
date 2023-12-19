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
    void *s_code_begin;
    size_t s_code_len;
    size_t s_entry_offset;
    dom_id_t dom_id;
    // TODO: pass info about the required data region size
};

struct ioctl_dom_call_args {
    dom_id_t dom_id;
    unsigned long retval;
};

#define IOCTL_DOM_CREATE			_IOWR(IOC_MAGIC, 0, struct ioctl_dom_create_args)
#define IOCTL_DOM_CALL  			_IOWR(IOC_MAGIC, 1, struct ioctl_dom_call_args)

#endif
