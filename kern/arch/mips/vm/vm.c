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

struct lock 			*giant_paging_lock;
struct spinlock			slk_steal = SPINLOCK_INITIALIZER;

static
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
	return -1;
}

void
tlb_unmap( vaddr_t vaddr ) {
	int		ix_tlb;
	uint32_t	tlb_hi;
	uint32_t	tlb_lo;

	
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

/**
 * kickstart the virtual memory system.
 * we simply initialize our coremap structure.
 */
void
vm_bootstrap( void ) {
	//botstrap the coremap.
	coremap_bootstrap();
	
	//create the giant paging lock.
	giant_paging_lock = lock_create( "giant_paging_lock" );
	if( giant_paging_lock == NULL ) 
		panic( "vm_bootstrap: could not create giant_paging_lock." );
}

int
vm_fault( int fault_type, vaddr_t fault_addr ) {
	struct addrspace		*as;
	
	//make sure it is page aligned.
	fault_addr &= PAGE_FRAME;

	//get the addrspace.
	as = curthread->t_addrspace;
	if( as == NULL ) 
		return EFAULT;

	//delegate the fault to the address space.
	return as_fault( as, fault_type, fault_addr );
}

void
vm_map( vaddr_t vaddr, paddr_t paddr, int writeable ) {
	int			ix;
	int			ix_tlb;
	uint32_t		tlb_hi;
	uint32_t		tlb_lo;

	//lock the coremap for atomicity.
	LOCK_COREMAP();
	
	//get the coremap_entry index associated with the paddr.
	ix = PADDR_TO_COREMAP( paddr );
	
	//probe to see if this virtual address is already mapped inside the tlb.
	ix_tlb = tlb_probe( vaddr, 0 );

	//if it is not
	if( ix_tlb < 0 ) {
		//make sure what the coremap has is correct.
		KASSERT( coremap[ix].cme_tlb_ix == -1 );
		KASSERT( coremap[ix].cme_cpu == 0 );
		
		//get a free tlb slot.
		ix_tlb = tlb_get_free_slot();
		KASSERT( ix_tlb >= 0 && ix_tlb < NUM_TLB );

		//update the coremap entry.
		coremap[ix].cme_tlb_ix = ix_tlb;
		coremap[ix].cme_cpu = curcpu->c_number;
	}
	else {	
		//make sure it reflects the stats we have in the coremap.
		KASSERT( coremap[ix].cme_tlb_ix == ix_tlb );
		KASSERT( coremap[ix].cme_cpu == curcpu->c_number );
	}
	
	//set the hi entry to be the first 20 bits of the vaddr.
	tlb_hi = vaddr & TLBHI_VPAGE;

	//set the V bit to true also, to signify that this mapping is valid.
	tlb_lo = (paddr & TLBLO_PPAGE) | TLBLO_VALID;

	//if the requested mapping is writeable, we must set the D bit.
	if( writeable )
		tlb_lo |= TLBLO_DIRTY;

	//write it to the tlb.
	tlb_write( tlb_hi, tlb_lo, ix_tlb );

	//unpin the frame.
	coremap[ix].cme_wired = 0;

	//this frame has been referenced.
	coremap[ix].cme_referenced = 1;

	//wake up eveyrone waiting on wc_wire.
	wchan_wakeall( wc_wire );

	//unlock the coremap.
	UNLOCK_COREMAP();
	
}

void
vm_unmap( vaddr_t vaddr ) {
	LOCK_COREMAP();
	tlb_unmap( vaddr );
	UNLOCK_COREMAP();
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
		
		coremap[ix_cme].cme_tlb_ix = -1;
		coremap[ix_cme].cme_cpu = 0;
		coremap[ix_cme].cme_referenced = 0;
	}
}

void
tlb_clear() {
	int 		i;

	COREMAP_IS_LOCKED();
	for( i = 0; i < NUM_TLB; ++i )
		tlb_invalidate( i );
}

void
tlb_invalidate_coremap_entry( unsigned ix ) {
	KASSERT( ix < cm_stats.cms_total_frames );
	if( coremap[ix].cme_tlb_ix != -1 ) 
		tlb_invalidate( coremap[ix].cme_tlb_ix );
	
	KASSERT( coremap[ix].cme_tlb_ix == -1 );
	KASSERT( coremap[ix].cme_cpu == 0 );
}
