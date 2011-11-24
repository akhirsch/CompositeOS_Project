/**
 * Copyright 2007 by Boston University.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Initial Author: Gabriel Parmer, gabep1@cs.bu.edu, 2007.
 */

#ifndef SPD_H
#define SPD_H

#ifndef ASM

//#include <cos_types.h>
//#include <consts.h>
#include "shared/cos_types.h"
#include "shared/consts.h"

/**
 * Service Protection Domains
 *
 * Protection domains for different services or components do not
 * consist of entire address spaces defined by hardware page tables.
 * Instead, a single address space is divided between all of the
 * services within an application's execution domain.  In a sense, an
 * application and all its services exist in a private single address
 * space.  However, this is not a single address space OS as ALL
 * services across the entire system for all applications exist in the
 * single address space.  
 *
 * This file defines the structures and values for defining service
 * protection domains.  
 */

//#define MAX_NUM_SPDS 32
/* ~ ((pagesize - sizeof(spd))/sizeof(static_capability)) */
//#define MAX_STATIC_CAP 1000

/*
 * Static capabilities:
 * 
 * The user-level version of the static capability. A vector of these
 * is mapped into each spd user-level address space.  It contains the
 * address of the function to invoke to make the ipc, and the address
 * of the function in the trusted service to call, a count of the
 * number of times invocations have been made with this capability,
 * and the capability number for this entry.  Be sure to keep this
 * synchronized with asm_ipc_defs.h.
 */
/* user half found in cos_types.h */

/* ST stubs are linked automatically */
struct usr_cap_stubs {
	vaddr_t ST_serv_entry;
	vaddr_t SD_cli_stub, SD_serv_stub;
	vaddr_t AT_cli_stub, AT_serv_stub;
};

#define CAP_FREE NULL
#define CAP_ALLOCATED_UNUSED ((void*)1)

/* 
 * Structure defining the information contained in a static capability
 * for invocation with trust.
 */
struct spd;
struct invocation_cap {
	/* the spd that can make invocations on this capability
	 * (owner), and the spd that invocations are made to. owner ==
	 * NULL means that this entry is free and not in use.  */
	struct spd *owner, *destination;
	unsigned int invocation_cnt:30;
	isolation_level_t il:2;
	vaddr_t dest_entry_instruction;
	/* 
	 * For now, this can be part of the structure as the structure
	 * should still remain <= 32 bytes, however if this changes,
	 * this should be removed into an array of user_cap_stubs.
	 * They are not used on the IPC path. 
	 */
	struct usr_cap_stubs usr_stub_info;
} CACHE_ALIGNED;


/* end static capabilities */


/*
 * The Service Protection Domain description including 
 *
 * - location information inside an address space, and a page table.
 * - size information regarding the static capabilities (max and
 *   current size)
 * - the identification permissions (which identification bytes this
 *   spd can control.)
 * - The address of the user-level static capabilities structure
 * - The static capabilities for this service.
 */

/* spd flags */
#define SPD_COMPOSITE   0x1 // Is this spd a composite_spd?
#define SPD_FREE        0x2 // currently unused
#define SPD_DEPRICATED  0x4 // Must have SPD_COMPOSITE.  This spd
                            // should no longer be used except for
			    // thread returns.
#define SPD_SUBORDINATE 0x8 // must be a spd_composite.  uses the page 
                            // table of another composite spd, so if we 
                            // delete this, do not delete the pgtbl, 
                            // just decriment the reference count of 
                            // the spd that has the pg_tbl (via master_spd ptr)
#define SPD_ACTIVE      0x10 // spd is active, can have capabilities
			     // assigned to it, and threads executed
			     // within it...
#define MAX_MPD_DESC 1024  // Max number of descriptors for composite spds

/*
 * The spd_poly struct contains all the information that both struct
 * spds and struct composite_spds contains.  In essence, we want them
 * to be polymorphic with spd_poly as the "base class".
 */
struct spd_poly {
	unsigned int flags;
	atomic_t ref_cnt;
	paddr_t pg_tbl;
};

/* 
 * A collection of Symmetrically trusted spds.
 *
 * Spd membership in this composite can be tested by seeing if a
 * specific spd's address range is present in the spd_info->pg_tbl
 *
 * 
 */
struct spd;
struct composite_spd {
	struct spd_poly spd_info;
	struct spd *members; // iff !(flags & (SPD_DEPRICATED|SPD_SUBORDINATE))
	struct composite_spd *master_spd; // iff flags & SPD_SUBORDINATE
	struct composite_spd *next, *prev; /* flags & SPD_FREE -> next == freelist |
					    * flags & SPD_SUBORDINATE -> list of subordinates */
} CACHE_ALIGNED;

/* 
 * Currently spds consist of a contiguous range of virtual
 * addresses. if lowest_addr == 0, then we share page tables with the
 * main configuration task, thus having universal memory access.
 */
struct spd_location {
	unsigned int lowest_addr;
	unsigned int size;
};

typedef int mmaps_t;

struct spd {
	/* data touched on the ipc hotpath (32 bytes)*/
	struct spd_poly spd_info;
	struct spd_location location[MAX_SPD_VAS_LOCATIONS];
	/* The "current" protection state of the spd, which might
	 * point directly to spd->spd_info, or
	 * a composite_spd->spd_info 
	 */
	struct spd_poly /*composite_spd*/ *composite_spd; 
	
	unsigned short int cap_base, cap_range;
	unsigned short int fault_handler[COS_NUM_FAULTS];
	/*
	 * user_cap_tbl is a pointer to the virtual address within the
	 * kernel address space of the user level capability table,
	 * while user_vaddr_cap_tbl is a pointer into actual
	 * component-space.
	 */
	struct usr_inv_cap *user_cap_tbl, *user_vaddr_cap_tbl;

	/* if this service is a scheduler, at what depth is it, and
	 * who's its parent? sched_depth < 0 if not schedulert */
	int sched_depth;
	struct spd *parent_sched;

	struct cos_sched_data_area *sched_shared_page, *kern_sched_shared_page;
	unsigned short int prev_notification;
	struct cos_net_xmit_headers *cos_net_xmit_headers;

	mmaps_t local_mmaps; /* mm_handle (see hijack.c) for linux compat */

	vaddr_t upcall_entry;
	
	vaddr_t atomic_sections[COS_NUM_ATOMIC_SECTIONS];

	/* should be a union to not waste space */
	struct spd *freelist_next;
	/* Linked list of the members of a non-depricated, current composite spd */
	struct spd *composite_member_next, *composite_member_prev;
        struct vas *composite_vas;
        struct vas_freelist nonfree;
} CACHE_ALIGNED; //cache line size

struct vas { 
  /* The layout of components in the vas */
  struct spd *virtual_spd_layout[PGD_PER_PTBL]; 
  /* Where the vas starts in Composite memory */
  unsigned int start_addr;
  /* Where the vas is in the array. */
  unsigned int vas_id;
  /*The freelist*/
  struct vas_freelist freelst;
};

struct vas_freelist {
  struct vas_freelist_node *fst;
  struct vas_freelist_node *lst;
};

struct vas_freelist_node {
  int ptr;
  struct vas_freelist_node *next;
};


/*
  VAS freelist struct, for keeping track of free places to put an spd.

  The node functions should only be used in the freelist code, the freelist functions should be used by everyone else.
 */
struct vas_freelist *vas_freelist_new(int);
struct vas_freelist_node *vas_freelist_node_new(int);
void vas_freelist_add(struct vas_freelist *, int);
int vas_freelist_pop(struct vas_freelist *);
void vas_freelist_node_free(struct vas_freelist_node*);
void vas_freelist_free(struct vas_freelist *);
int vas_freelist_pop_largest(struct vas_freelist *);

/*VAS SYSCALL FUNCTIONS*/
int vas_new(); /*Creates a new vas and adds it to the vas list*/
int vas_delete(int); /*Given a vas id, deletes that vas*/
int vas_spd_add(int, struct spd *); /*Given a vas id and a spd, adds the spd to the vas*/
int vas_spd_remove(int, struct spd *); /*Given a vas id and a spd, removes the spd from the vas*/
int vas_expand(int, struct spd *); /*Given a vas id and a spd, gives that spd an extra 4MB space.*/
int vas_retract(int, struct spd *); /*Given a vas id and a spd, retracts that spd in that vas.*/

paddr_t spd_alloc_pgtbl(void);
void spd_free_pgtbl(paddr_t pa);
struct spd *spd_alloc(unsigned short int max_static_cap, struct usr_inv_cap *usr_cap_tbl, 
		      vaddr_t upcall_entry);
int spd_set_location(struct spd *spd, unsigned long lowest_addr, 
		     unsigned long size, paddr_t pg_tbl);
int spd_add_location(struct spd *spd, long base, long size);
int spd_rem_location(struct spd *spd, long base, long size);
void spd_free(struct spd *spd);

int spd_is_free(int idx);
extern struct spd spds[MAX_NUM_SPDS];
static inline int spd_get_index(struct spd *spd)
{
//	return ((unsigned long)spd-(unsigned long)spds)/sizeof(struct spd);
	return (int)(spd-&spds[0]);
}
struct spd *spd_get_by_index(int idx);
void spd_free_all(void);
void spd_init(void);

int spd_reserve_cap_range(struct spd *spd, int amnt);
int spd_release_cap_range(struct spd *spd);

int spd_cap_activate(struct spd *spd, int cap);
int spd_cap_set_dest(struct spd *spd, int cap, struct spd* dspd);
int spd_cap_set_cstub(struct spd *spd, int cap, vaddr_t fn);
int spd_cap_set_sstub(struct spd *spd, int cap, vaddr_t fn);
int spd_cap_set_sfn(struct spd *spd, int cap, vaddr_t fn);
int spd_cap_set_fault_handler(struct spd *spd, int cap, int handler_num);

unsigned int spd_add_static_cap(struct spd *spd, vaddr_t service_entry_inst, struct spd *trusted_spd, 
				isolation_level_t isolation_level);
unsigned int spd_add_static_cap_extended(struct spd *spd, struct spd *trusted_spd, 
					 int cap_offset, vaddr_t ST_entry_fn,
					 vaddr_t AT_cli_stub, vaddr_t AT_serv_stub,
					 vaddr_t SD_cli_stub, vaddr_t SD_serv_stub,
					 isolation_level_t isolation_level, int flags);
isolation_level_t cap_change_isolation(int cap_num, isolation_level_t il, int flags);
int cap_is_free(int cap_num);
unsigned long spd_read_reset_invocation_cnt(struct spd *cspd, struct spd *sspd);
struct invocation_cap *inv_cap_get(int c_num);

static inline int spd_is_scheduler(struct spd *spd)
{
	return spd->sched_depth >= 0;
}
static inline int spd_is_root_sched(struct spd *spd)
{
	return spd->sched_depth == 0;
}
static inline int spd_is_member(struct spd *spd, struct composite_spd *cspd)
{ 
	return spd->composite_spd == &cspd->spd_info;
}
static inline int spd_is_composite(struct spd_poly *info)
{ 
	return info->flags & SPD_COMPOSITE;
}
static inline int spd_is_active(struct spd *s)
{
	return s->spd_info.flags | SPD_ACTIVE;
}
static inline void spd_make_active(struct spd *s)
{
	s->spd_info.flags |= SPD_ACTIVE;
}
static inline int spd_mpd_is_depricated(struct composite_spd *mpd)
{ 
	return (mpd)->spd_info.flags & SPD_DEPRICATED;
}
static inline int spd_mpd_is_subordinate(struct composite_spd *mpd)
{ 
	return (mpd)->spd_info.flags & SPD_SUBORDINATE;
}

static inline void spd_mpd_reset_flags(struct composite_spd *mpd)
{
	mpd->spd_info.flags = 0;
}

static inline void spd_mpd_set_flags(struct composite_spd *mpd, int flags)
{
	mpd->spd_info.flags |= flags;
}

static inline void spd_mpd_remove_flags(struct composite_spd *mpd, int flags)
{
	mpd->spd_info.flags &= ~flags;
}

void spd_init_mpd_descriptors(void);
short int spd_alloc_mpd_desc(void);

void spd_mpd_release_desc(short int desc);
void spd_mpd_release(struct composite_spd *cspd);
static inline void spd_mpd_take(struct composite_spd *cspd)
{
	cos_ref_take(&cspd->spd_info.ref_cnt);
#ifdef __KERNEL__
//	printk("cos: take %p (%d)\n", cspd, cos_ref_val(&cspd->spd_info.ref_cnt));
#endif
}

struct composite_spd *spd_mpd_by_idx(short int idx);
short int spd_mpd_index(struct composite_spd *cspd);
static inline void spd_mpd_depricate(struct composite_spd *mpd)
{
	//printk("cos: depricating cspd %d.\n", spd_mpd_index(mpd));
	spd_mpd_set_flags(mpd, SPD_DEPRICATED);
	spd_mpd_release(mpd);
}
void spd_mpd_make_subordinate(struct composite_spd *master, struct composite_spd *slave);
static inline struct composite_spd *spd_alloc_mpd(void)
{
	return spd_mpd_by_idx(spd_alloc_mpd_desc());
}
static inline void spd_mpd_ipc_release(struct composite_spd *cspd)
{
	cos_meas_event(COS_MPD_IPC_REFCNT_DEC); 
	spd_mpd_release(cspd);
}
static inline void spd_mpd_ipc_take(struct composite_spd *cspd)
{
	cos_meas_event(COS_MPD_IPC_REFCNT_INC); 
	spd_mpd_take(cspd);
}

int spd_composite_add_member(struct composite_spd *cspd, struct spd *spd);
int spd_composite_remove_member(struct spd *spd, int remove_mappings);

/* FIXME: keep a count in the cspd to avoid this iteration */
static inline int spd_composite_num_members(struct composite_spd *cspd) 
{
	int num_members = 0;
	struct spd *iter;

	iter = cspd->members;
	if (iter) do {
		num_members++;
		iter = iter->composite_member_next;
		//assert(iter);
	} while (iter != cspd->members);

	return num_members;
}

/*
 * Move spd from its current composite spd to the cspd_new
 * composite_spd.
 */
static inline int spd_composite_move_member(struct composite_spd *cspd_new, struct spd *spd, int remove_mappings)
{
	/* 
	 * FIXME: in a multi-processor context, these should be
	 * probably be done in the opposite order.
	 */
	if (spd_composite_remove_member(spd, remove_mappings) ||
	    spd_composite_add_member(cspd_new, spd)) {
		return -1;
	}

	return 0;
}

struct spd *virtual_namespace_query(unsigned long addr);
int virtual_namespace_alloc(struct spd *spd, unsigned long addr, unsigned int size);

/*
 * FIXME: this should take a range of addresses, but since each
 * component < 4MB here, that is unneeded.
 *
 * FIXME: TEST THIS!
 */
extern int pgtbl_entry_absent(vaddr_t addr, paddr_t pg_tbl);
static inline int spd_composite_member(struct spd *spd, struct spd_poly *poly)
{
	vaddr_t lowest_addr = spd->location[0].lowest_addr;

	return !pgtbl_entry_absent(poly->pg_tbl, lowest_addr);
}

#else /* ASM */

/* WRONG */
#define SPD_CAP_TBL_PTR 8
#define SPD_CAP_TBL_SZ 12
#define SPD_LOWER_ADDR 0
#define SPD_REGION_SIZE 4


#define CAP_SIZE 8
#define CAP_ENTRY_INST 0
#define CAP_SPD 4

#endif

#endif /* SPD_H */
