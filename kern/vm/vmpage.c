#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spinlock.h>
#include <synch.h>
#include <thread.h>
#include <addrspace.h>
#include <synch.h>
#include <vm.h>
#include <vm/page.h>
#include <vm/region.h>
#include <vm/swap.h>
#include <current.h>
#include <machine/coremap.h>

struct lock		*lk_transit;
struct cv		*cv_transit;

static
int
vm_page_new( struct vm_page **vmp_ret, paddr_t *paddr_ret ) {
	struct vm_page		*vmp;
	paddr_t			paddr;

	vmp = vm_page_create();
	if( vmp == NULL )
		return ENOMEM;
	
	//attempt to allocate swap space.
	vmp->vmp_swapaddr = swap_alloc();
	if( vmp->vmp_swapaddr == INVALID_SWAPADDR ) {
		vm_page_destroy( vmp );
		return ENOSPC;
	}

	//allocate a single coremap_entry 
	paddr = coremap_alloc( vmp, true );
	if( paddr == INVALID_PADDR ) {
		vm_page_destroy( vmp );
		return ENOSPC;
	}

	//page is already wired, now just lock it.
	vm_page_lock( vmp );
		
	//adjust the physical address, and mark the page dirty.
	vmp->vmp_paddr = paddr;

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

	//release the swap space if it exists.
	if( vmp->vmp_swapaddr != INVALID_SWAPADDR )
		swap_dealloc( vmp->vmp_swapaddr );

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

	source_paddr = source->vmp_paddr & PAGE_FRAME;

	//if the source page is not in core, swap it in.
	if( source_paddr == INVALID_PADDR ) {
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

		//make sure nobody paged-in this page.
		KASSERT( (source->vmp_paddr & PAGE_FRAME) == INVALID_PADDR );

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
	
	//initialize both the physical address
	//and swap address to be invalid.
	vmp->vmp_paddr = INVALID_PADDR;
	vmp->vmp_swapaddr = INVALID_SWAPADDR;
	vmp->vmp_in_transit = false;

	return vmp;
}

int
vm_page_new_blank( struct vm_page **ret ) {
	struct vm_page		*vmp;
	paddr_t			paddr;
	int			res;
	
	res = vm_page_new( &vmp, &paddr );
	if( res )
		return res;
	
	//make sure the page is locked.
	VM_PAGE_IS_LOCKED( vmp );

	//unlock the page.
	vm_page_unlock( vmp );

	//zero the paddr and unwire it
	coremap_zero( paddr );
	coremap_unwire( paddr );

	*ret = vmp;
	return 0;
}

int
vm_page_fault( struct vm_page *vmp, struct addrspace *as, int fault_type, vaddr_t fault_vaddr ) {
	paddr_t		paddr;
	int		writeable;
	off_t		swap_addr;
	bool		need_transit;

	need_transit = false;
	(void) as;


	//get the transit lock.
	lock_acquire( lk_transit );

	//we lock the page for atomicity.
	vm_page_lock( vmp );

	//if our page is being swapped out by someone else ...
	while( vmp->vmp_in_transit ) {
		vm_page_unlock( vmp );
		cv_wait( cv_transit, lk_transit );
		vm_page_lock( vmp );
	}
		
	KASSERT( vmp->vmp_in_transit == false );

	//get the physical address.
	paddr = vmp->vmp_paddr & PAGE_FRAME;

	//if the page is in core
	if( paddr != INVALID_PADDR ) {
		//we don't need the transit lock.
		lock_release( lk_transit );

		//wire the coremap entry associated
		coremap_wire( paddr );	
	} 
	else {
		//we need to unlock the transit lock at the end.
		need_transit = true;
		
		//mark the page as being in transit.
		vmp->vmp_in_transit = true;
	
		swap_addr = vmp->vmp_swapaddr;
		KASSERT( vmp->vmp_swapaddr != INVALID_SWAPADDR );
		
		//release the transit lock.
		lock_release( lk_transit );

		paddr = coremap_alloc( vmp, true );
		if( paddr == INVALID_PADDR ) {
			vmp->vmp_in_transit = false;
			vm_page_unlock( vmp );
			return ENOMEM;
		}
		
		//unlock the page and transit.
		vm_page_unlock( vmp );

		//swap the page in.
		swap_in( paddr, swap_addr );

		//when we come back make sure nothing really hot changed.
		KASSERT( vmp->vmp_paddr == INVALID_PADDR );
		KASSERT( vmp->vmp_swapaddr == swap_addr );
		KASSERT( vmp->vmp_in_transit );

		//reacquire the locks.
		lock_acquire( lk_transit );
		vm_page_lock( vmp );

		//update the physical address.
		vmp->vmp_paddr = paddr;
		vmp->vmp_in_transit = false;
	}

	//which fault happened?
	switch( fault_type ) {
		case VM_FAULT_READ:	
			writeable = 0;
			break;
		case VM_FAULT_WRITE:
		case VM_FAULT_READONLY:
			writeable = 1;
			break;
		default:
			coremap_unwire( paddr );
			vm_page_unlock( vmp );
			lock_release( lk_transit );
			return EINVAL;
		
	}

	//map fault_vaddr into paddr with writeable flags.
	vm_map( fault_vaddr, paddr, writeable );

	//unwire the coremap entry.
	coremap_unwire( paddr );

	//unlock the page.
	vm_page_unlock( vmp );

	//release transit lock
	if( need_transit ) {
		cv_broadcast( cv_transit, lk_transit );
		lock_release( lk_transit );
	}

	return 0;
}

/**
 * evict the page from core.
 */
bool
vm_page_evict( struct vm_page *victim ) {
	paddr_t		paddr;
	off_t		swap_addr;

	//lock the transit.
	lock_acquire( lk_transit );

	//lock the page.
	vm_page_lock( victim );
	
	//mark the page as being in transit.
	victim->vmp_in_transit = true;
	paddr = victim->vmp_paddr & PAGE_FRAME;

	//allocate swap space.
	KASSERT( victim->vmp_swapaddr != INVALID_SWAPADDR );
	
	swap_addr = victim->vmp_swapaddr;

	//unlock and swap out.
	vm_page_unlock( victim );

	//release the transit lock.
	lock_release( lk_transit );

	//swap the page out.
	swap_out( paddr, swap_addr );
	
	//reacquire the transit lock.
	lock_acquire( lk_transit );

	//update the page information.
	vm_page_lock( victim );
	KASSERT( victim->vmp_in_transit );

	victim->vmp_paddr = INVALID_PADDR;
	victim->vmp_in_transit = false;
	vm_page_unlock( victim );
	
	//wake anyone possibly waiting.
	cv_broadcast( cv_transit, lk_transit );
	lock_release( lk_transit );

	return true;
}
	
