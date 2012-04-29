#include <types.h>
#include <lib.h>
#include <copyinout.h>
#include <proc.h>
#include <thread.h>
#include <current.h>
#include <filedesc.h>
#include <addrspace.h>
#include <kern/errno.h>
#include <vm/page.h>
#include <vm/region.h>
#include <machine/trapframe.h>
#include <synch.h>
#include <syscall.h>

int	
sys_sbrk( intptr_t inc, void **ptr ) {
	struct vm_region		*vmr_heap;
	struct addrspace		*as;
	vaddr_t				heap_end;
	vaddr_t				heap_start;
	unsigned			new_pages;
	unsigned			current_pages;
	unsigned			covered_range;
	int				res;

	KASSERT( curthread != NULL && curthread->td_proc != NULL );

	as = curthread->t_addrspace;
	heap_start = as->as_heap_start;

	if( inc == 0 ) {
		*ptr = (void*)heap_start;
		as->as_heap_end = heap_start;
		return 0;
	}

	inc = ROUNDUP(inc,4);

	//get the region responsible for the heap.
	vmr_heap = vm_region_find_responsible( as, heap_start );
	if( vmr_heap == NULL )
		panic( "sbrk() : could not find heap region." );

	//sanity check.
	KASSERT( vmr_heap->vmr_base == heap_start );
	
	//calculate the end of the heap.
	current_pages = vm_page_array_num( vmr_heap->vmr_pages );
	heap_end = as->as_heap_end;
	covered_range = heap_start + current_pages * PAGE_SIZE;

	//if the increase is negative, make sure we wont go below the starting point.
	if( heap_end + inc < heap_start ) {
		*ptr = ((void*)-1);
		return EINVAL;
	}

	//if we can cover this range without allocation, we are safe.
	if( heap_end + inc < covered_range ) {
		as->as_heap_end += inc;
		*ptr = (void*)heap_end;
		return 0;
	}

	new_pages = DIVROUNDUP(((heap_end + inc)-heap_start),PAGE_SIZE);
	if( new_pages >= PROC_MAX_HEAP_PAGES ) {
		*ptr = ((void*)-1);
		return ENOMEM;
	}
		
	res = vm_region_resize( vmr_heap, new_pages );
	if( res ) {
		*ptr = ((void*)-1);
		return res;
	}

	*ptr = (void*)heap_end;
	as->as_heap_end += inc;
	return 0;
}
	
