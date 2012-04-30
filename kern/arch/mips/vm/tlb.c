#include <types.h>
#include <synch.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <cpu.h>
#include <spl.h>
#include <wchan.h>
#include <current.h>
#include <machine/coremap.h>
#include <vm.h>
#include <addrspace.h>
#include <machine/tlb.h>

int
tlb_get_free_slot() {
	int 		i;
	uint32_t	tlb_hi;
	uint32_t	tlb_lo;
	int		spl;

	//loop over all possible tlb entries.
	spl = splhigh();
	for( i = 0; i < NUM_TLB; ++i ) {
		//read the current entry.
		tlb_read( &tlb_hi, &tlb_lo, i );	

		//if it is a valid entry, continue.
		if( tlb_lo & TLBLO_VALID )
			continue;
		
		//return interrupts level to what they were.
		splx( spl );

		//found a good entry.
		return  i;
	}
	
	splx( spl );
	return tlb_evict();
}

void
tlb_unmap( vaddr_t vaddr ) {
	int		ix_tlb;
	uint32_t	tlb_hi;
	uint32_t	tlb_lo;

	COREMAP_IS_LOCKED();
	//probe the tlb for the given vaddr.
	ix_tlb = tlb_probe( vaddr, 0 );

	//if it does not exist, then there's nothing to unmap.
	if( ix_tlb < 0 )
		return;
	
	//read the tlb entry.
	tlb_read( &tlb_hi, &tlb_lo, ix_tlb );

	//make sure it is a valid mapping.
	KASSERT( tlb_lo & TLBLO_VALID );

	//invalidate the entry.
	tlb_invalidate( ix_tlb );
}

void
tlb_invalidate( int ix_tlb ) {
	uint32_t		tlb_lo;
	uint32_t		tlb_hi;
	paddr_t			paddr;
	unsigned		ix_cme;

	KASSERT( ix_tlb >= 0 && ix_tlb < NUM_TLB );

	COREMAP_IS_LOCKED();

	//read the tlb entry given by ix_tlb.
	tlb_read( &tlb_hi, &tlb_lo, ix_tlb );

	//invalidate it.
	tlb_write( TLBHI_INVALID( ix_tlb ), TLBLO_INVALID() , ix_tlb );

	//check to see that the entry retrieved was valid.
	if( tlb_lo & TLBLO_VALID ) {
		//get the physical address mapped to it.
		paddr = tlb_lo & TLBLO_PPAGE;
	
		//convert to coremap index.
		ix_cme = PADDR_TO_COREMAP( paddr );
		
		KASSERT(coremap[ix_cme].cme_tlb_ix == ix_tlb);
		KASSERT(coremap[ix_cme].cme_cpu == curcpu->c_number);
	}

	coremap[ix_cme].cme_tlb_ix = -1;
	coremap[ix_cme].cme_cpu = 0;
}

/**
 * clear the current tlb, by invalidating every single entry.
 */
void
tlb_clear() {
	int 		i;

	COREMAP_IS_LOCKED();
	for( i = 0; i < NUM_TLB; ++i )
		tlb_invalidate( i );
}


/**
 * Choose a random entry to evict from the tlb.
 */
int
tlb_evict( void ) {
	int		tlb_victim;
	
	COREMAP_IS_LOCKED();
	tlb_victim = random() % NUM_TLB;
	tlb_invalidate( tlb_victim );
	
	return tlb_victim;
}

/**
 * go to sleep in wc_shootdown until someone wakes us up.
 */
void
tlb_shootdown_wait( ) {
	wchan_lock( wc_shootdown );
	UNLOCK_COREMAP();
	wchan_sleep( wc_shootdown );
	LOCK_COREMAP();
}
