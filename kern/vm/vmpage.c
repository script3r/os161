#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spinlock.h>
#include <synch.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>
#include <vm/page.h>
#include <vm/region.h>
#include <vm/swap.h>
#include <machine/coremap.h>

static
void
vm_page_acquire( struct vm_page *vmp ) {
	(void)vmp;
}

void
vm_page_destroy( struct vm_page *vmp ) {
	paddr_t		paddr;

	vm_page_acquire( vmp );
	paddr = vmp->vmp_paddr & PAGE_FRAME;
	
	//if the page is in core.
	if( VM_PAGE_IN_CORE( vmp ) ) {
		//invalidate it
		vmp->vmp_paddr = INVALID_PADDR;

		//unlock and free the coremap entry associated
		vm_page_unlock( vmp );
		coremap_free( paddr, false );

		//must unwire what was wired by acquire.
		coremap_unwire( paddr );
	} 
	else {
		//the physical address is already invalid ...
		//just unlock, so we can free the page.
		vm_page_unlock( vmp );
	}

	spinlock_cleanup( &vmp->vmp_slk );
	kfree( vmp );
}
