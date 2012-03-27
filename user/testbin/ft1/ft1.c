#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

int main( int argc, char *argv[] ) {
	int 		fd_stdout = 0;
	
	(void)argc;
	(void)argv;

	//attempt to open stdout
	fd_stdout = open( "con:", O_WRONLY );
	if( fd_stdout == -1 )
		return -1;

	//now we can write 
	char message[] = "Hello World!";
	write( fd_stdout, message, sizeof( message ) );
	
	//close the file
	close( fd_stdout );

	return 0;
}
