#ifndef __CAPSTONE_H_
#define __CAPSTONE_H_

#define CAPSTONE_DEV_PATH "/dev/capstone"

// ioctl code
#define IOC_MAGIC '\xb8'

struct ioctl_dom_create_args {
    // virtual region containing code
    void *code_begin;
    size_t code_len;
    size_t entry_offset;
    // TODO: pass info about the required data region size
};

#define IOCTL_DOM_CREATE			_IOR(IOC_MAGIC, 0, struct ioctl_dom_create_args)

#endif
