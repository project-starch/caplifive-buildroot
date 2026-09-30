#ifndef __LIB_CAPSTONE_H__
#define __LIB_CAPSTONE_H__

#include "../../include/capstone.h"

int capstone_init();
int capstone_process_init(void);
int capstone_process_stats(struct ioctl_process_stats *stats);
int capstone_cleanup();
/* Informational loader output is enabled by default for legacy probes. */
void capstone_set_verbose(int enabled);
/* Checked variants for process launchers: -1 with errno on failure. */
int capstone_call(dom_id_t domain, unsigned long *result);
int capstone_step(dom_id_t domain, struct ioctl_dom_step_args *step);
int capstone_share(dom_id_t domain, region_id_t region,
                   unsigned long permission, unsigned long revocation);
/* Translated mappings (managed API): GRANT turns a region of ours into a
   PRIVATE mapping of the domain and returns its binding word; RELEASE ends
   it. -1 with errno on failure (EINVAL, EPERM, EBUSY, ENOSPC, ENOENT, EIO). */
int capstone_map_grant(dom_id_t domain, region_id_t region, unsigned long len,
                       unsigned long prot, unsigned long *binding);
int capstone_map_release(dom_id_t domain, unsigned long binding);


dom_id_t create_dom(const char *c_path, const char *s_path);
dom_id_t create_dom_ko(const char *c_path, const char *s_path);
unsigned long call_dom(dom_id_t dom_id);
region_id_t create_region(unsigned long len);
void shared_region_annotated(dom_id_t dom_id, region_id_t region_id, unsigned long annotation_perm, unsigned long annotation_rev);
void share_child_region(dom_id_t dom_id, region_id_t parent_id, unsigned long offset, unsigned long len, unsigned long annotation_perm);
void share_region(dom_id_t dom_id, region_id_t region_id);
void revoke_region(region_id_t region_id);
/* revoke a region of ours; 0: popped and freed as well, 1: kept below a slot the monitor holds, -1: refused */
int release_region(region_id_t region_id);
void *map_region(region_id_t region_id, unsigned long len);
void probe_regions(void);
int region_count(void);
void schedule_dom(dom_id_t dom_id);

#endif
