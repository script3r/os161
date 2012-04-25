/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <vm.h>
#include <vm/region.h>
#include <vm/page.h>
#include <array.h>
#include <machine/coremap.h>
#include <current.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */

DEFARRAY_BYTYPE( vm_region_array ,struct vm_region, /* ... */);

struct addrspace *
as_create(void)
{
	struct addrspace *as;

	as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}

	//create the array of regions.
	as->as_regions = vm_region_array_create();
	if( as->as_regions == NULL ) {
		kfree( as );
		return NULL;
	}

	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace 	*newas;
	struct vm_region 	*vmr;
	struct vm_region	*newvmr;
	unsigned		i;
	int			result;

	newas = as_create();
	if (newas==NULL) {
		return ENOMEM;
	}

	//copy all vm regions that reside in the old addrspace.
	for( i = 0; i < vm_region_array_num( old->as_regions ); ++i ) {
		vmr = vm_region_array_get( old->as_regions, i );

		//if clone fails, we simply return the reason
		//it failed, after destroying newas.
		result = vm_region_clone( vmr, &newvmr );
		if( result ) {
			as_destroy( newas );
			return result;
		}
	}
	
	*ret = newas;
	return 0;
}

void
as_destroy(struct addrspace *as)
{
	struct vm_region		*vmr;
	unsigned			i;

	//destroy each vm region associated with this addrspace.
	for( i = 0; i < vm_region_array_num( as->as_regions ); ++i ) {
		vmr = vm_region_array_get( as->as_regions, i );
		vm_region_destroy( vmr );
	}

	//reside the regions array to 0, and
	//destroy the array.
	vm_region_array_setsize( as->as_regions, 0 );
	vm_region_array_destroy( as->as_regions );
	kfree( as );
}

void
as_activate(struct addrspace *as)
{
	KASSERT( as != NULL || curthread->t_addrspace == as );
	LOCK_COREMAP();
	tlb_clear();
	UNLOCK_COREMAP();
}

static 
bool
as_overlaps_region( struct addrspace *as, size_t sz, vaddr_t vaddr ) {
	unsigned		i;
	vaddr_t			bottom;
	vaddr_t			top;
	struct vm_region	*vmr;

	//loop over all regions.
	for( i = 0; i <  vm_region_array_num( as->as_regions ); ++i ) {
		//get the current region.
		vmr = vm_region_array_get( as->as_regions, i );

		//calculate bottom and top virtual addresses.
		bottom = vmr->vmr_base;
		top = bottom + vm_page_array_num( vmr->vmr_pages ) * PAGE_SIZE;

		//if the tail & head of the new vm_region are inside
		//the current address space block,then we have an overlap.
		if( vaddr + sz > bottom && vaddr < top )
			return true;
	}

	return false;
}
/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
		 int readable, int writeable, int executable)
{
	struct vm_region		*vmr;
	int				res;

	(void) readable;
	(void) writeable;
	(void) executable;

	//align the virtual address.
	vaddr &= PAGE_FRAME;	
	
	//round up the size so it is a multiple of the page size.
	sz = ROUNDUP( sz, PAGE_SIZE );
	
	//if there's an overlap, well we have a problem.
	if( as_overlaps_region( as, sz, vaddr ) )
		return EINVAL;
	
	//create a region with the specified amount of pages.
	vmr = vm_region_create( sz / PAGE_SIZE );
	if( vmr == NULL )
		return ENOMEM;

	vmr->vmr_base = vaddr;
	//add it to the addresspace.
	res = vm_region_array_add( as->as_regions, vmr, NULL );
	if( res ) {
		vm_region_destroy( vmr );
		return res;
	}
	return 0;
}

int
as_prepare_load(struct addrspace *as)
{
	(void)as;
	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	(void)as;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	int			err;

	//create the stack region.
	err = as_define_region( as, USERSTACKBASE, USERSTACKSIZE, 1, 1, 0 );
	if( err )
		return err;

	*stackptr = USERSTACK;
	return 0;
}

int
as_fault( struct addrspace *as, int fault_type, vaddr_t fault_addr ) {
	struct vm_region		*vmr;
	int				ix_page;
	struct vm_page			*vmp;
	int				res;

	KASSERT( as != NULL );

	//find the responsible vm_region for the faulty address.
	vmr = vm_region_find_responsible( as, fault_addr );
	if( vmr == NULL )
		return EFAULT;

	//find the responsible vm_page.
	ix_page = (fault_addr - vmr->vmr_base) / PAGE_SIZE;
	
	//get the virtual page.
	vmp = vm_page_array_get( vmr->vmr_pages, ix_page );
	
	//if the virtual page is null, it means we have to zero-fill it.
	if( vmp == NULL ) {
		//create  a new blank page
		res = vm_page_new_blank( &vmp );
		if( res ) 
			return res;
		
		//append to to the region.
		vm_page_array_set( vmr->vmr_pages, ix_page, vmp );
	}
	return vm_page_fault( vmp, as, fault_type, fault_addr );
}
