/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

/* 
 * This file is included by both the kernel and by components.  Thus
 * any defines might need to be, unfortunately, made using ifdefs
 */

#ifndef CONSTS_H
#define CONSTS_H

#ifndef __ASM__
#ifdef __KERNEL__
#include <linux/thread_info.h> /* for PAGE_SIZE */
#else 
struct pt_regs {
        long bx;
        long cx;
        long dx;
        long si;
        long di;
        long bp;
        long ax;
        long ds;
        long es;
        long fs;
        long gs;
        long orig_ax;
        long ip;
        long cs;
        long flags;
        long sp;
        long ss;
};
//struct pt_regs { int dummy[16]; };
#endif
#endif
#define PAGE_ORDER 12
#ifndef __KERNEL__
#define PAGE_SIZE (1<<PAGE_ORDER)
#endif

#define MAX_SERVICE_DEPTH 31
#define MAX_NUM_THREADS   30
/* Stacks are 2 * page_size (expressed in words) */
#define MAX_STACK_SZ    (PAGE_SIZE/4) /* a page */
#define ALL_STACK_SZ    (MAX_NUM_THREADS*MAX_STACK_SZ)
#define MAX_SPD_VAS_LOCATIONS 8
#define COS_NUM_FAULTS 1

/* a kludge:  should not use a tmp stack on a stack miss */
#define TMP_STACK_SZ       (128/4) 
#define ALL_TMP_STACKS_SZ  (MAX_NUM_THREADS*TMP_STACK_SZ)

#define MAX_SCHED_HIER_DEPTH 4

#define MAX_NUM_SPDS   64
#define MAX_STATIC_CAP 1024

#define PAGE_MASK    (~(PAGE_SIZE-1))
#define PGD_SHIFT    22
#define PGD_RANGE    (1<<PGD_SHIFT)
#define PGD_SIZE     PGD_RANGE
#define PGD_MASK     (~(PGD_RANGE-1))
#define PGD_PER_PTBL 1024

#define round_to_pow2(x, pow2)    (((unsigned long)(x))&(~(pow2-1)))
#define round_up_to_pow2(x, pow2) (round_to_pow2(((unsigned long)x)+pow2-1, pow2))

#define round_to_page(x)        round_to_pow2(x, PAGE_SIZE)
#define round_up_to_page(x)     round_up_to_pow2(x, PAGE_SIZE)
#define round_to_pgd_page(x)    round_to_pow2(x, PGD_SIZE)
#define round_up_to_pgd_page(x) round_up_to_pow2(x, PGD_SIZE)

#define CACHE_LINE (32)
#define CACHE_ALIGNED __attribute__ ((aligned (CACHE_LINE)))
#define HALF_CACHE_ALIGNED __attribute__ ((aligned (CACHE_LINE/2)))
#define PAGE_ALIGNED __attribute__ ((aligned(PAGE_SIZE)))
#define WORD_SIZE 32

#define round_to_cacheline(x)    round_to_pow2(x, CACHE_LINE)
#define round_up_to_cacheline(x) round_up_to_pow2(x, CACHE_LINE)

#define SHARED_REGION_START (1<<30)  // 1 gig
#define SHARED_REGION_SIZE PGD_RANGE
#define SERVICE_START (SHARED_REGION_START+SHARED_REGION_SIZE)
#define SERVICE_END   ((unsigned long)SHARED_REGION_START+(unsigned long)(1<<30))
/* size of virtual address spanned by one pgd entry */
#define SERVICE_SIZE PGD_RANGE
#define COS_INFO_REGION_ADDR SHARED_REGION_START
#define COS_DATA_REGION_LOWER_ADDR (COS_INFO_REGION_ADDR+PAGE_SIZE)
#define COS_DATA_REGION_MAX_SIZE (MAX_NUM_THREADS*PAGE_SIZE)

#define COS_NUM_ATOMIC_SECTIONS 10

#define COS_MAX_MEMORY 2048

#include "../asm_ipc_defs.h"

#define KERN_BASE_ADDR 0xc0000000 //CONFIG_PAGE_OFFSET

#endif
