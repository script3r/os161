#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>

int main( int argc, char *argv[] ) {
	const char	*args[] = { "foo", "os161", "execv", NULL };
		
	(void) argc;
	(void) argv;

	printf( "about to call execv\n" );
	execv( "/testbin/ft1", (char **)args );

	return 0;
}
