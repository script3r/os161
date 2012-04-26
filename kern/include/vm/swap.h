#ifndef _VM_SWAP_H
#define _VM_SWAP_H

#include <bitmap.h>

#define INVALID_SWAPADDR 0
#define SWAP_DEVICE "lhd0raw:"
#define SWAP_MIN_FACTOR 2

#define LOCK_SWAP() (lock_acquire(lk_sw))
#define UNLOCK_SWAP() (lock_release(lk_sw))

/**
 * holds statistics regarding swapping.
 * ss_total: total number of pages we can hold.
 * ss_free: free pages count.
 * ss_reserved: how many of them are reserved.
 */
struct swap_stats {
	unsigned int		ss_total;
	unsigned int		ss_free;
	unsigned int		ss_reserved;
	unsigned int		ss_used;
};

void		swap_bootstrap( void );
off_t		swap_alloc(void);
void		swap_in( paddr_t, off_t );
void		swap_out( paddr_t, off_t );

#endif
