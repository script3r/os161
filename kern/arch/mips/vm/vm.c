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
#include <vm/swap.h>
#include <vm/page.h>
#include <addrspace.h>
#include <machine/tlb.h>

struct spinlock			slk_steal = SPINLOCK_INITIALIZER;

/**
 * kickstart the virtual memory system.
 * we simply initialize our coremap structure.
 */
void
vm_bootstrap( void ) {
	//botstrap the coremap.
	coremap_bootstrap();
	
	//make sure to bootstrap our swap.
	swap_bootstrap();
}

int
vm_fault( int fault_type, vaddr_t fault_addr ) {
	struct addrspace		*as;
	int				res;

	//make sure it is page aligned.
	fault_addr &= PAGE_FRAME;

	//get the addrspace.
	as = curthread->t_addrspace;
	if( as == NULL ) 
		return EFAULT;

	//delegate the fault to the address space.
	res = as_fault( as, fault_type, fault_addr );
	KASSERT( !lock_do_i_hold( giant_paging_lock ) );
	return res;
}

void
vm_map( vaddr_t vaddr, paddr_t paddr, int writeable ) {
	int			ix;
	int			ix_tlb;
	uint32_t		tlb_hi;
	uint32_t		tlb_lo;

	KASSERT( (paddr & PAGE_FRAME) == paddr );
	KASSERT( paddr != INVALID_PADDR );

	//lock the coremap for atomicity.
	LOCK_COREMAP();
	
	//get the coremap_entry index associated with the paddr.
	ix = PADDR_TO_COREMAP( paddr );
	
	KASSERT( coremap_is_wired( paddr ) );

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

	//unlock the coremap.
	UNLOCK_COREMAP();
	
}

void
vm_unmap( vaddr_t vaddr ) {
	LOCK_COREMAP();
	tlb_unmap( vaddr );
	UNLOCK_COREMAP();
}

