#ifndef _VM_SWAP_H
#define _VM_SWAP_H

#define INVALID_SWAPADDR 0
#define SWAP_DEVICE "lhd0raw:"


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
};

void		swap_bootstrap( void );
off_t		swap_alloc( void );
void		swap_free( off_t );
int		swap_reserve( unsigned int );
void		swap_unreserve( unsigned int );
void		swap_in( paddr_t, off_t );
void		swap_out( paddr_t, off_t );

extern struct lock 		*giant_paging_lock;

#endif
