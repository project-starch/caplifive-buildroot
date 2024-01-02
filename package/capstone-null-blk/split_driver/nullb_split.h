typedef __u32 __bitwise blk_opf_t;

enum req_op {
	/* read sectors from the device */
	REQ_OP_READ		= (__force blk_opf_t)0,
	/* write sectors to the device */
	REQ_OP_WRITE		= (__force blk_opf_t)1,
	/* flush the volatile write cache */
	REQ_OP_FLUSH		= (__force blk_opf_t)2,
	/* discard sectors */
	REQ_OP_DISCARD		= (__force blk_opf_t)3,
	/* securely erase sectors */
	REQ_OP_SECURE_ERASE	= (__force blk_opf_t)5,
	/* write the zero filled sector many times */
	REQ_OP_WRITE_ZEROES	= (__force blk_opf_t)9,
	/* Open a zone */
	REQ_OP_ZONE_OPEN	= (__force blk_opf_t)10,
	/* Close a zone */
	REQ_OP_ZONE_CLOSE	= (__force blk_opf_t)11,
	/* Transition a zone to full */
	REQ_OP_ZONE_FINISH	= (__force blk_opf_t)12,
	/* write data at the current zone write pointer */
	REQ_OP_ZONE_APPEND	= (__force blk_opf_t)13,
	/* reset a zone write pointer */
	REQ_OP_ZONE_RESET	= (__force blk_opf_t)15,
	/* reset all the zone present on the device */
	REQ_OP_ZONE_RESET_ALL	= (__force blk_opf_t)17,

	/* Driver private requests */
	REQ_OP_DRV_IN		= (__force blk_opf_t)34,
	REQ_OP_DRV_OUT		= (__force blk_opf_t)35,

	REQ_OP_LAST		= (__force blk_opf_t)36,
};


