#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

int main( int argc, char *argv[] ) {
	(void)argc;
	(void)argv;

	printf( "my pid is %d\n", getpid() );
	return 0;
}
