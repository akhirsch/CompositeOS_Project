/**
 * Copyright 2010 by The George Washington University.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gparmer@gwu.edu, 2010
 */

#ifndef   	MEM_MGR_H
#define   	MEM_MGR_H

/* Map a physical frame into a component. */
vaddr_t mman_get_page(spdid_t spd, vaddr_t addr, int flags);
/* 
 * remove this single mapping _and_ all descendents.  FIXME: this can
 * be called with the spdid of a dependent component.  We should also
 * pass in the component id of the calling component to ensure that it
 * is allowed to remove the designated page.
 */
void mman_release_page(spdid_t spd, vaddr_t addr, int flags);
/* remove all descendent mappings of this one (but not this one). */ 
void mman_revoke_page(spdid_t spd, vaddr_t addr, int flags); 
/* The invoking component (s_spd) must own the mapping. */
vaddr_t mman_alias_page(spdid_t s_spd, vaddr_t s_addr, spdid_t d_spd, vaddr_t d_addr);
void mman_print_stats(void);

#endif 	    /* !MEM_MGR_H */
