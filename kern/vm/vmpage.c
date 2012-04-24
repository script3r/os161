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
	paddr_t		paddr;
	paddr_t		wired;

	wired = INVALID_PADDR;

	//lock the page.
	vm_page_lock( vmp );
	for( ;; ) {
		//get the physical address
		paddr = vmp->vmp_paddr & PAGE_FRAME;

		//if the physcal address matches the wired address
		//we are done.
		if( paddr == wired )
			break;
		

		//unlock the page.
		vm_page_unlock( vmp );
	
		//that means we pinned this physical addr before
		if( wired != INVALID_PADDR )
			coremap_unwire( paddr );
		
		
		//check if the page has been paged out.
		if( paddr == INVALID_PADDR ) {
			vm_page_lock( vmp );
			break;
		}
		
		coremap_wire( paddr );
		wired = paddr;
		vm_page_lock( vmp );
	}
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

void
vm_page_lock( struct vm_page *vmp ) {
	spinlock_acquire( &vmp->vmp_slk );
}

void
vm_page_unlock( struct vm_page *vmp ) {
	spinlock_release( &vmp->vmp_slk );
}

