#ifndef __CAPSTONE_SBI_H_
#define __CAPSTONE_SBI_H_


#define SBI_EXT_CAPSTONE 0x12345678

#define SBI_EXT_CAPSTONE_DOM_CREATE 0x0
#define SBI_EXT_CAPSTONE_DOM_CALL   0x1
#define SBI_EXT_CAPSTONE_DOM_CALL_WITH_CAP   0x2
#define SBI_EXT_CAPSTONE_REGION_CREATE   0x3
#define SBI_EXT_CAPSTONE_REGION_SHARE    0x4
#define SBI_EXT_CAPSTONE_DOM_RETURN      0x5 
#define SBI_EXT_CAPSTONE_REGION_QUERY    0x6
#define SBI_EXT_CAPSTONE_DOM_SCHEDULE    0x7
#define SBI_EXT_CAPSTONE_REGION_COUNT    0x8
#define SBI_EXT_CAPSTONE_REGION_SHARE_ANNOTATED    0x9
#define SBI_EXT_CAPSTONE_REGION_REVOKE    0xa

#define CAPSTONE_REGION_FIELD_BASE    0x0
#define CAPSTONE_REGION_FIELD_END     0x1
#define CAPSTONE_REGION_FIELD_LEN     0x2

#endif
