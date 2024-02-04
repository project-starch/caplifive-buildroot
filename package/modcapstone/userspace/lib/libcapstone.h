#ifndef __LIB_CAPSTONE_H__
#define __LIB_CAPSTONE_H__

#include "../../include/capstone.h"

int capstone_init();
int capstone_cleanup();


dom_id_t create_dom(const char *c_path, const char *s_path);
dom_id_t create_dom_ko(const char *c_path, const char *s_path);
unsigned long call_dom(dom_id_t dom_id);
region_id_t create_region(unsigned long len);
void shared_region_annotated(dom_id_t dom_id, region_id_t region_id, unsigned long annotation_perm, unsigned long annotation_rev);
void share_region(dom_id_t dom_id, region_id_t region_id);
void revoke_region(region_id_t region_id);
void *map_region(region_id_t region_id, unsigned long len);
void probe_regions(void);
int region_count(void);
void schedule_dom(dom_id_t dom_id);

#endif
