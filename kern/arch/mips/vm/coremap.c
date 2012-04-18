#include <types.h>
#include <lib.h>
#include <machine/coremap.h>
#include <synch.h>
#include <wchan.h>
#include <thread.h>
#include <vm.h>
#include <current.h>

struct coremap_stats		cm_stats;
struct coremap_entry		*coremap;
struct wchan			*wc_wire;
struct wchan			*wc_shootdown;
struct spinlock			slk_coremap = SPINLOCK_INITIALIZER;

/**
 * initialize the statistics given the first and last physical addresses
 * that we are responsible for.
 */
static
void
coremap_init_stats( paddr_t first, paddr_t last ) {
	cm_stats.cms_base = first / PAGE_SIZE;
	cm_stats.cms_total_frames = last / PAGE_SIZE - cm_stats.cms_base;
	cm_stats.cms_kpages = 0;
	cm_stats.cms_upages = 0;
	cm_stats.cms_free = cm_stats.cms_total_frames;
	cm_stats.cms_wired = 0;
}

/**
 * initializes the coremap entry residing on index "ix".
 */
static
void
coremap_init_entry( unsigned int ix ) {
	KASSERT( ix < cm_stats.cms_total_frames );

	coremap[ix].cme_kernel = 0;
	coremap[ix].cme_last = 0;
	coremap[ix].cme_alloc = 0;
	coremap[ix].cme_referenced = 0;
	coremap[ix].cme_wired = 0;
}

/**
 * initialize our coremap data structure.
 * we figure out how much physical core memory we have to manage,
 * then allocate memory for our coremap by stealing it from the ram,
 * finally, we initialize each of our coremap_entries.
 */
void
coremap_bootstrap( void ) {
	paddr_t			first;		//first physical address
	paddr_t			last;		//last physucal addr
	uint32_t		nframes;	//total number of frames
	size_t			nsize;		//size of coremap
	uint32_t		i;		
	
	//the number of frames we have to manage.
	nframes = (last - first) / PAGE_SIZE;
	
	//calculate the necessary size and round it up
	//to the nearest page size.
	nsize = nframes * sizeof( struct coremap_entry );
	nsize = ROUNDUP( nsize, PAGE_SIZE );

	//now, actually steal the memory.
	//the kernel is directly mapped, so we simply convert the physical address using
	//the PADDR_TO_KVADDR macro.
	coremap = (struct coremap_entry *) PADDR_TO_KVADDR( first );

	//advance the first address, since we just stole memory.
	first += nsize;

	//initialize our stats.
	coremap_init_stats( first, last );
	
	//initialize each coremap entry.
	for( i = 0; i < cm_stats.cms_total_frames; ++i ) 
		coremap_init_entry( i );

	//create the waiting channel for those 
	//that are waiting to wire a certain frame.
	wc_wire = wchan_create( "wc_wire" );
	if( wc_wire == NULL )
		panic( "coremap_bootstrap: could not create wc_wire" );
	
	//create the waiting channel for those
	//who are waiting for the shootdown to be complete.
	wc_shootdown = wchan_create( "wc_shootdown" );
	if( wc_shootdown == NULL )
		panic( "coremap_bootstrap: could not create wc_shootdown" );
}

/**
 * this functiond decides whether a given coremap_entry index
 * is free, or it is allocated.
 */
static
bool
coremap_is_free( int ix ) {
	return !coremap[ix].cme_wired && 
	       !coremap[ix].cme_alloc && 
               !coremap[ix].cme_desired;
}

static
bool
coremap_is_pageable( int ix ) {
	return !coremap[ix].cme_wired &&
	       !coremap[ix].cme_desired &&
	       !coremap[ix].cme_kernel;
}

static
int
rank_region_for_paging( int ix, int size ) {
	int 		score;
	int		i;

	score = 0;
	for( i = ix; i < ix + size; ++i ) {
		if( !coremap_is_pageable( i ) )
			return -1;
		
		if( coremap_is_free( i ) )
			++score;
	}
		
	return score;
}

/**
 * finds an optimal range inside the coremap
 * to allocate npages. the optimal range is one that requires the least amount of evictions.
 */
static
int
find_optimal_range( int npages ) {
	int 		best_base;
	int		best_count;
	int		curr_count;
	uint32_t	i;

	best_count = -1;
	best_base = -1;
	
	for( i = 0; i < cm_stats.cms_total_frames - npages; ++i ) {
		curr_count = rank_region_for_paging( i, npages );
		if( curr_count > best_count ) {
			best_base = i;
			best_count = curr_count;
		}
	}

	return best_base;
}

/**
 * finds a page that could be paged-out.
 */
static
int
find_pageable_page( void ) {
	uint32_t	i;

	for( i = 0; i < cm_stats.cms_total_frames; ++i )
		if( coremap_is_pageable( i ) )
			return i;

	panic( "find_pageable_page: no pageable pages were found." );
	return -1;
}

static
void
do_evict( int ix ) {
	(void)ix;
}


static
int
do_page_replace( void ) {
	int		ix;

	//find a page that we could evict.
	ix = find_pageable_page();
	
	//if we need to evict it ... then do it.
	if( !coremap_is_free( ix ) ) 
		do_evict( ix );
	
	return ix;
}

static
paddr_t
coremap_alloc_single( struct vm_page *vmp, bool wired ) {
	int				ix;
	int				i;

	//lock the coremap for atomicity.
	LOCK_COREMAP();

	//so far we don't know any index.
	ix = -1;

	//check to see if we have a free page.
	if( cm_stats.cms_free > 0 ) {
		for( i = cm_stats.cms_total_frames; i >= 0; --i ) {
			if( coremap_is_free( i ) ) {
				ix = i;
				break;
			}
		}
	}
	
	//at this point, two things could happen.
	//either, ix still is -1, which means we couldn't find a single free page.
	//or it contains a valid address.

	//if we are not in an interrupt, we simply try to evict a page.
	if( ix < 0 && curthread != NULL && !curthread->t_in_interrupt )
		ix = do_page_replace();
	

	//if the index is still negative, it means that
	//there's nothing to do anymore, we cannot grab a page.
	if( ix < 0 ) {
		UNLOCK_COREMAP();
		return INVALID_PADDR;
	}

	//mark the page we just got as allocated.
	//and if we had a virtual page associated, then store it inside the coremap.
	mark_pages_as_allocated( ix, 1, wired, ( vmp == NULL ) );

	UNLOCK_COREMAP();
	return COREMAP_TO_PADDR(ix);
}

/**
 * Mark pages as allocated.
 * Coremap must already be locked.
 */
void
mark_pages_as_allocated( int start, int num, bool wired, bool is_kernel ) {
	int 		i;

	//go over each page in the range
	//and mark them as allocated.
	for( i = start; i < start + num; ++i ) {
		coremap[i].cme_alloc = 1;
		coremap[i].cme_wired = ( wired ) ? 1 : 0;
		coremap[i].cme_kernel = ( is_kernel ) ? 1 : 0;
		coremap[i].cme_referenced = 1;
	}

	//update statistics
	if( is_kernel )
		cm_stats.cms_kpages += num;
	else
		cm_stats.cms_upages += num;

	//we have less free pages now.
	cm_stats.cms_free -= num;
}

/**
 * Mark a range of pages as desired.
 * Coremap must be locked.
 */
static
void
mark_pages_desiredness( int start, int num, bool desired ) {
	int 		i;
	
	for( i = start; i < start + num; ++i ) {
		KASSERT( !coremap[i].cme_desired );
		coremap[i].cme_desired = ( desired ) ? 1 : 0;
	}
}

static
paddr_t
coremap_alloc_multipages( int npages ) {
	int			ix;
	int			i;

	//lock the coremap for atomicity.
	LOCK_COREMAP();
	
	//find the optimal range to store npages.
	//the optimal range is simply the range that has the least amount
	//if evictions.
	ix = find_optimal_range( npages );
	
	//if we couldn't find a range ... too bad.
	if( ix < 0 ) {
		UNLOCK_COREMAP();
		return INVALID_PADDR;
	}

	//mark that range as desired, before we start evicting.
	//otherwise, other processes may jump in.
	mark_pages_desiredness( ix, npages, true );

	//now, we evict those pages that need to be evicted.
	for( i = ix; i < ix + npages; ++i ) {
		if( coremap[i].cme_alloc ) {
			//if we can evict, oh well, then just do it.
			if( curthread != NULL && !curthread->t_in_interrupt ) {
				do_evict( i );
			}
			else {
				//if we are here, we can't evict a page.
				//therefore we mark our previous pages as undesired.
				mark_pages_desiredness( ix, npages, false );
				UNLOCK_COREMAP();
				return INVALID_PADDR;
			}
		}	
	}

	//at this point, the entire range we choose is ours.
	//so we simply mark them as allocated.
	mark_pages_as_allocated( ix, npages, false, true );

	//unlock the coremap and proceed with life.
	UNLOCK_COREMAP();

	return COREMAP_TO_PADDR( ix );
}

static
void
tlb_invalidate_coremap_entry( int ix ) {

}

/**
 * allocate kernel pages
 */
vaddr_t
alloc_kpages( int npages ) {
	paddr_t		paddr;		//the physical addr of the allocated page
	
	//if we have multiple allocation requests, then call the multi-version.
	//otherwise, simply allocate a single page.
	( npages > 1 ) ? 
		coremap_alloc_multipages( npages ) : coremap_alloc_single( NULL, 0 );
	
	//if we have an invalid physical address
	//return 0 as a virtual address, which is not possible.
	if( paddr == INVALID_PADDR )
		return 0;

	return PADDR_TO_KVADDR( paddr );
}

/**
 * free a series of pages.
 */
void
free_kpages( vaddr_t vaddr ) {
	coremap_free( KVADDR_TO_PADDR( vaddr ), true );
}

/**
 * free a kernel coremap allocation.
 */
void
coremap_free( paddr_t paddr, bool is_kernel ) {
	uint32_t		i;
	uint32_t		ix;

	//convert the given physical address into the appropriate
	//physical frame.
	ix = PADDR_TO_COREMAP( paddr );
	
	//lock the coremap for atomicity.
	LOCK_COREMAP();

	//we loop over starting from ix, until possibly the end.
	for( i = ix; i < cm_stats.cms_total_frames; ++i ) {
		//make sure the page is actually allocated.
		//further, make sure it is wired or is a kernel page.
		KASSERT( coremap[i].cme_alloc == 1 );
		KASSERT( coremap[i].cme_wired || is_kernel );
		
		//invalidate the given c
		tlb_invalidate_coremap_entry( i );

		//mark it as deallocated and update stats.
		coremap[i].cme_alloc = 0;
		coremap[i].cme_kernel ? --cm_stats.cms_kpages : --cm_stats.cms_upages;
		coremap[i].cme_referenced = 0;
		coremap[i].cme_page = NULL;
	
		//one extra free page.
		++cm_stats.cms_free;
	
		//if we are the last in a series of allocations, bail.
		if( coremap[i].cme_last ) {
			coremap[i].cme_last = 0;
			break;
		}
	}

	UNLOCK_COREMAP();
}
