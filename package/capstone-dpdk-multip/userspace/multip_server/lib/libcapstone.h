#ifndef __LIB_CAPSTONE_H__
#define __LIB_CAPSTONE_H__

#include "capstone.h"

/**
 * Values used by the custom communication protocol
 * The server and each client domain should use the exact same values for expected protocol communication
*/
#define SERVER_GET 0xacef  /* Signals the server that a client produced something that can be consumed */
#define CLIENT_PUT 0xcadd  /* signals the client that the server placed some information in the shared region */
#define ACK 0xaccc
/* ################################################ */

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

