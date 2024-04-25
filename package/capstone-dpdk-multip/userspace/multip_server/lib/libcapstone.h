#ifndef __LIB_CAPSTONE_H__
#define __LIB_CAPSTONE_H__

#include "capstone.h"

#define CAPSTONE_ANNOTATION_PERM_IN 0x0
#define CAPSTONE_ANNOTATION_PERM_INOUT 0x1
#define CAPSTONE_ANNOTATION_PERM_OUT 0x2
#define CAPSTONE_ANNOTATION_PERM_EXE 0x3
#define CAPSTONE_ANNOTATION_PERM_FULL 0x4

#define CAPSTONE_ANNOTATION_REV_DEFAULT 0x0
#define CAPSTONE_ANNOTATION_REV_BORROWED 0x1
#define CAPSTONE_ANNOTATION_REV_SHARED 0x2
#define CAPSTONE_ANNOTATION_REV_TRANSFERRED 0x3

#define ACK 0x0
#define SERVER_SEND 0x1
#define SERVER_RECEIVE 0x2

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
