#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>

int main( int argc, char *argv[] ) {
	int		i;

	printf( "\ngot %d arguments\n", argc );
	printf( "the address of argv is: %p\n", argv );
	
	for( i = 0; i < argc; ++i )
		printf( "argv[%d] = %s\n", i, argv[i] );
	
	return 0;
}
