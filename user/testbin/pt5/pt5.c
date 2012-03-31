#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>

int main( int argc, char *argv[] ) {
	printf( "got %d arguments\n", argc );
	printf( "the address of argv is: %p\n", argv );
	printf( "the address of argv[0] is: %p", argv[0] );
	printf( "the first argument is: %s\n", argv[0] );

	return 0;
}
