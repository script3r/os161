#include <types.h>
#include <synch.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <current.h>
#include <machine/coremap.h>
#include <vm.h>
#include <addrspace.h>

struct lock 			*giant_paging_lock;
struct spinlock			slk_steal = SPINLOCK_INITIALIZER;

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

	return as_fault( as, fault_type, fault_addr );
}	
