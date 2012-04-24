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
int
vm_page_new( struct vm_page **vmp_ret, paddr_t *paddr_ret ) {
	struct vm_page		*vmp;
	paddr_t			paddr;

	vmp = vm_page_create();
	if( vmp == NULL )
		return ENOMEM;
	
	//allocate a single coremap_entry 
	paddr = coremap_alloc( vmp, true );
	if( paddr == INVALID_PADDR ) {
		vm_page_destroy( vmp );
		return ENOSPC;
	}

	//page is already wired, now just lock it.
	vm_page_lock( vmp );
		
	//adjust the physical address, and mark the page dirty.
	vmp->vmp_paddr = paddr | VM_PAGE_DIRTY;

	*vmp_ret = vmp;
	*paddr_ret = paddr;

	return 0;
}

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

int
vm_page_clone( struct vm_page *source, struct vm_page **target ) {
	struct vm_page		*vmp;
	int			res;
	paddr_t			paddr;
	paddr_t			source_paddr;
	off_t			swap_addr;

	//create a new vm_page
	res = vm_page_new( &vmp, &paddr );
	if( res )
		return res;

	//acquire the source page.
	vm_page_acquire( source );

	//if the source page is not in core
	//we must swap it in.
	if( !VM_PAGE_IN_CORE( source ) ) {
		//get the swap offset of the source page.
		swap_addr = source->vmp_swapaddr;
		
		//unlock the source page.
		vm_page_unlock( source );
		
		//alocate memory for the source page.
		source_paddr = coremap_alloc( source, true );
		if( source_paddr == INVALID_PADDR ) {
			//unwire the page, since it was wired by vm_page_new.
			coremap_unwire( vmp->vmp_paddr & PAGE_FRAME );

			//destroy it.
			vm_page_destroy( vmp );
			return ENOMEM;
		}
	
		//swap in the contents located ins swap_addr into source_paddr.
		swap_in( source_paddr, swap_addr );

		//lock the source.
		vm_page_lock( source );
		
		//adjust the physical address to reflect the 
		//address that currently stores the swapped in content.
		source->vmp_paddr = source_paddr;
	}
	
	//clone from source to the new address.
	coremap_clone( source_paddr, paddr );
	
	//unlock both source and target.
	vm_page_unlock( source );
	vm_page_unlock( vmp );

	//unwire both pages.
	coremap_unwire( source_paddr );
	coremap_unwire( paddr );

	*target = vmp;
	return 0;
}

struct vm_page *
vm_page_create( ) {
	struct vm_page		*vmp;

	vmp = kmalloc( sizeof( struct vm_page ) );
	if( vmp == NULL )
		return NULL;
	
	spinlock_init( &vmp->vmp_slk );

	vmp->vmp_paddr = INVALID_PADDR;
	vmp->vmp_swapaddr = INVALID_SWAPADDR;

	return vmp;
}

int
vm_page_new_blank( struct vm_page **ret ) {
	(void)ret;
	return ENOMEM;
}

int
vm_page_fault( struct vm_page *vmr, struct addrspace *as, int fault_type, vaddr_t fault_vaddr ) {
	(void)vmr;
	(void)as;	
	(void)fault_type;
	(void)fault_vaddr;
	return EFAULT;
}
