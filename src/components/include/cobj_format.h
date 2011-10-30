/**
 * Copyright 2010 by Gabriel Parmer and The George Washington
 * University.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gparmer@gwu.edu, 2010
 */

#ifndef   	COBJ_FORMAT_H
#define   	COBJ_FORMAT_H

#include <cos_types.h>

/* 
 * Layout: 
 *
 * header
 * section 0
 * section 1 (zeros)
 * section N
 * symb 0
 * symb 1
 * symb M
 * cap 0
 * cap 1
 * cap K
 * section 0 body
 * section N body
 *
 * Pending additions: 1) exported function names, 2) undefined function
 * names, 3) thread state
 */

/* Currently assume that nsect == 3, data, text, bss */
#define COBJ_NSECT 3

struct cobj_header {
	u32_t id, nsect, nsymb, ncap, size;
} __attribute__((packed));

#define COBJ_SECT_UNINIT 0
#define COBJ_SECT_READ   0x1
#define COBJ_SECT_WRITE  0x2
#define COBJ_SECT_ZEROS  0x8

struct cobj_sect {
	u32_t flags;
	u32_t offset;
	u32_t vaddr, bytes;
} __attribute__((packed));

enum {
	COBJ_SYMB_UNDEF = 0,
	COBJ_SYMB_COMP_INFO,
	COBJ_SYMB_EXPORTED
};

struct cobj_symb {
	u32_t type;
	u32_t vaddr;
} __attribute__((packed));

struct cobj_cap { 
	u32_t cap_off, dest_id, fault_num;
	u32_t sfn, cstub, sstub;
} __attribute__((packed));

static inline int
cobj_cap_is_fault(struct cobj_cap *c) { return c->fault_num <= COS_NUM_FAULTS; }

u32_t cobj_size_req(u32_t nsect, u32_t sect_sz, u32_t nsymb, u32_t ncap);
struct cobj_header *cobj_create(u32_t id, u32_t nsect, u32_t sect_sz, u32_t nsymb, u32_t ncap, 
				char *space, unsigned int sz);

int cobj_sect_init(struct cobj_header *h, unsigned int sect_idx, u32_t flags, u32_t vaddr, u32_t size);
int cobj_symb_init(struct cobj_header *h, unsigned int symb_idx, u32_t type, u32_t vaddr);
int cobj_cap_init(struct cobj_header *h, unsigned int cap_idx, u32_t cap_off, 
		  u32_t dest_id, u32_t sfn, u32_t cstub, u32_t sstub, u32_t fault_num);

struct cobj_sect *cobj_sect_get(struct cobj_header *h, unsigned int sect_id);
struct cobj_symb *cobj_symb_get(struct cobj_header *h, unsigned int symb_id);
struct cobj_cap *cobj_cap_get(struct cobj_header *h, unsigned int cap_id);

int cobj_sect_empty(struct cobj_header *h, unsigned int sect_id);
u32_t cobj_sect_content_offset(struct cobj_header *h);
char *cobj_sect_contents(struct cobj_header *h, unsigned int sect_id);
u32_t cobj_sect_size(struct cobj_header *h, unsigned int sect_id);
u32_t cobj_sect_addr(struct cobj_header *h, unsigned int sect_id);

static inline int cobj_cap_undef(struct cobj_cap *c) 
{
	return 0 == c->sfn;
}

#endif 	    /* !COBJ_FORMAT_H */
