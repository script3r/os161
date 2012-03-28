#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#define BUF_SIZE	128

int main( int argc, char *argv[] ) {
	int		fd_out;
	int 		err;

	(void)argc;
	(void)argv;

	//try to open a sample output file.
	fd_out = open( "/hello.txt", O_WRONLY );
	if( fd_out < 0 ) {
		printf( "open: error opening the file." );
		return -1;
	}
	
	//close stdout.
	err = close( STDOUT_FILENO );
	if( err ) {
		printf( "close: could not close stdout." );
		return -1;
	}

	//route stdout to fd_out.
	dup2( fd_out, STDOUT_FILENO );
	
	//print a sample message to STDOUT.
	//this should end up inside the file.
	printf( "I should be seeing this message inside hello.txt\n" );
	
	//close fd_out
	err = close( fd_out );
	if( err )
		return err;
	
	//stdout will be closed by exit().
	return 0;
}
