#include <types.h>
#include <synch.h>
#include <lib.h>
#include <machine/coremap.h>
#include <vm.h>

struct lock 			*giant_paging_lock;
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


