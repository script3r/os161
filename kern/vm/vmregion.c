#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <array.h>
#include <addrspace.h>
#include <vm.h>
#include <vm/region.h>
#include <vm/swap.h>
#include <vm/page.h>
#include <machine/coremap.h>

DEFARRAY_BYTYPE( vm_page_array, struct vm_page, /* no inline */ );

struct vm_region *
vm_region_create( size_t npages ) {
	int 			res;
	struct vm_region	*vmr;
	unsigned		i;

	//attempt to create the vm_region
	vmr = kmalloc( sizeof( struct vm_region ) );
	if( vmr == NULL )
		return NULL;

	
	//create the vm_pages.
	vmr->vmr_pages = vm_page_array_create();
	if( vmr->vmr_pages == NULL ) {
		kfree( vmr );
		return NULL;
	}

	//set the base address to point to an invalid virtual address.
	vmr->vmr_base = INVALID_VADDR;

	//adjust the array to hold npages.
	res = vm_page_array_setsize( vmr->vmr_pages, npages );
	if( res ) {
		vm_page_array_destroy( vmr->vmr_pages );
		kfree( vmr );
		return NULL;
	}

	//initialize all the pages to NULL.
	for( i = 0; i < npages; ++i ) {
		vm_page_array_set( vmr->vmr_pages, i, NULL );
	}

	return vmr;
}

void
vm_region_destroy( struct addrspace *as, struct vm_region *vmr ) {
	//resize the vm region to 0.
	KASSERT( vm_region_resize( as, vmr, 0 ) == 0 );

	//destroy the pages associated with the region.
	vm_page_array_destroy( vmr->vmr_pages );

	kfree( vmr );
}

static
int
vm_region_shrink( struct addrspace *as, struct vm_region *vmr, unsigned npages ) {
	unsigned		i;
	struct vm_page		*vmp;

	for( i = npages; i < vm_page_array_num( vmr->vmr_pages ); ++i ) {
		vmp = vm_page_array_get( vmr->vmr_pages, i );
		if( vmp == NULL )
			continue;

		//unmap tlb entries.
		vm_unmap( as, vmr->vmr_base + PAGE_SIZE * i );

		//destroy the page.
		vm_page_destroy( vmp );	
	}

	return vm_page_array_setsize( vmr->vmr_pages, npages );
}

static
int
vm_region_expand( struct vm_region *vmr, unsigned npages ) {
	unsigned		new_pages;
	int			res;
	unsigned		i;

	new_pages = npages - vm_page_array_num( vmr->vmr_pages );

	//trivial case, nothing to do.
	if( new_pages == 0 )
		return 0;

	//attempt to rezize the vmr_pages array.
	res = vm_page_array_setsize( vmr->vmr_pages, npages );
	if( res )
		return res;


	//initialize each of the newly created vm_pages to NULL.
	for( i = vm_page_array_num( vmr->vmr_pages ); i < npages; ++i )
		vm_page_array_set( vmr->vmr_pages, i, NULL );
	
	return 0;
}

int			
vm_region_resize( struct addrspace *as, struct vm_region *vmr, unsigned npages ) {
	KASSERT( as != NULL );
	KASSERT( vmr != NULL );
	KASSERT( vmr->vmr_pages != NULL );

	if( npages < vm_page_array_num( vmr->vmr_pages ) )
		return vm_region_shrink( as, vmr, npages );
	return vm_region_expand( vmr, npages );
}
