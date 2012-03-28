#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#define BUF_SIZE	128

int main( int argc, char *argv[] ) {
	int 		fd_stdout;
	int		fd_in;
	char		buf[BUF_SIZE];
	int		nread;

	(void)argc;
	(void)argv;

	//attempt to open stdout
	fd_stdout = open( "con:", O_WRONLY );
	if( fd_stdout == -1 )
		return -1;

	//attempt to open input file
	fd_in = open( "/hello.txt", O_RDONLY );
	if( fd_in == -1 ) {
		close( fd_stdout );
		return -1;
	}
	
	//read and output to stdout
	while( ( nread = read( fd_in, buf, sizeof( buf ) ) ) > 0 )
		write( fd_stdout, buf, nread );
	
	//close the files
	close( fd_in );
	close( fd_stdout );

	return 0;
}
