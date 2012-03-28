#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#define BUF_SIZE	128

static
int
test_chdir( const char *path ) {
	int 		err;

	putchar( '\n' );

	printf( "attempting to switch directories to %s\n", path );
	err = chdir( path );
	if( err ) {
		printf( "failed to switch directories to %s\n", path );
		return -1;
	}
	
	printf( "successfully switched to %s\n", path );
	return 0;
}

int main( int argc, char *argv[] ) {
	(void)argc;
	(void)argv;
	
	if( test_chdir( "/testbin" ) )
		return -1;

	if( test_chdir( "/" ) )
		return -1;

	
	return 0;
}
