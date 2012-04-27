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
	int			err;

	//see if we can reserve npages of swap first.
	err = swap_reserve( npages );
	if( err )
		return NULL;

	//attempt to create the vm_region
	vmr = kmalloc( sizeof( struct vm_region ) );
	if( vmr == NULL )
		return NULL;

	//create the vm_pages.
	vmr->vmr_pages = vm_page_array_create();
	if( vmr->vmr_pages == NULL ) {
		kfree( vmr );
		swap_unreserve( npages );
		return NULL;
	}

	//set the base address to point to an invalid virtual address.
	vmr->vmr_base = 0xdeadbeef;

	//adjust the array to hold npages.
	res = vm_page_array_setsize( vmr->vmr_pages, npages );
	if( res ) {
		vm_page_array_destroy( vmr->vmr_pages );
		kfree( vmr );
		swap_unreserve( npages );
		return NULL;
	}

	//initialize all the pages to NULL.
	for( i = 0; i < npages; ++i )
		vm_page_array_set( vmr->vmr_pages, i, NULL );

	return vmr;
}

void
vm_region_destroy( struct vm_region *vmr ) {
	//resize the vm region to 0.
	KASSERT( vm_region_resize( vmr, 0 ) == 0 );

	//destroy the pages associated with the region.
	vm_page_array_destroy( vmr->vmr_pages );

	//free the memory
	kfree( vmr );
}

static
int
vm_region_shrink( struct vm_region *vmr, unsigned npages ) {
	unsigned		i;
	struct vm_page		*vmp;

	for( i = npages; i < vm_page_array_num( vmr->vmr_pages ); ++i ) {
		vmp = vm_page_array_get( vmr->vmr_pages, i );
		if( vmp == NULL ) {
			swap_unreserve( 1 );
			continue;
		}

		//unmap tlb entries.
		vm_unmap( vmr->vmr_base + PAGE_SIZE * i );

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

	//see if we can back this loan with storage.
	res = swap_reserve( new_pages );
	if( res )
		return res;

	//attempt to rezize the vmr_pages array.
	res = vm_page_array_setsize( vmr->vmr_pages, npages );
	if( res ) {
		swap_unreserve( new_pages );
		return res;
	}


	//initialize each of the newly created vm_pages to NULL.
	for( i = vm_page_array_num( vmr->vmr_pages ); i < npages; ++i )
		vm_page_array_set( vmr->vmr_pages, i, NULL );
	
	return 0;
}

int			
vm_region_resize( struct vm_region *vmr, unsigned npages ) {
	KASSERT( vmr != NULL );
	KASSERT( vmr->vmr_pages != NULL );

	if( npages < vm_page_array_num( vmr->vmr_pages ) )
		return vm_region_shrink( vmr, npages );
	return vm_region_expand( vmr, npages );
}

int
vm_region_clone( struct vm_region *source, struct vm_region **target ) {
	struct vm_region		*vmr;
	unsigned			i;
	struct vm_page			*vmp;
	struct vm_page			*vmp_clone;
	int				res;

	//create a new vm_region with the same amount of pages
	//as the previous one.
	vmr = vm_region_create( vm_page_array_num( source->vmr_pages ) );
	if( vmr == NULL )
		return ENOMEM;

	//copy the base.
	vmr->vmr_base = source->vmr_base;

	//loop over each of the pages
	for( i = 0; i < vm_page_array_num( source->vmr_pages ); ++i ) {
		vmp = vm_page_array_get( source->vmr_pages, i );
		vmp_clone = vm_page_array_get( vmr->vmr_pages, i );

		//if the page from the old addrspace is null, we dont
		//have anything to do.
		if( vmp == NULL )
			continue;
	
		//clone the page.
		res = vm_page_clone( vmp, &vmp_clone );	
		if( res ) {
			vm_region_destroy( vmr );
			return res;
		}	

		//now that we have the cloned page, add it to
		//the addrspace array
		vm_page_array_set( vmr->vmr_pages, i, vmp_clone );
	}
	
	//copy to the given pointer
	*target = vmr;
	return 0;
}

struct vm_region *
vm_region_find_responsible( struct addrspace *as, vaddr_t vaddr ) {
	unsigned		ix;
	struct vm_region	*vmr;
	vaddr_t			top;
	vaddr_t			bottom;

	//loop over all possible regions.
	for( ix = 0; ix < vm_region_array_num( as->as_regions ); ++ix ) {
		//get the vm_region
		vmr = vm_region_array_get( as->as_regions, ix );
		
		//calculate top and bottom
		bottom = vmr->vmr_base;	
		top = vmr->vmr_base + vm_page_array_num( vmr->vmr_pages ) * PAGE_SIZE;
		
		//if the virtual address is between bottom and top
		//thats the vm_region we are looking for.
		if( vaddr >= bottom && vaddr < top )
			return vmr;
	}
	return NULL;
}

