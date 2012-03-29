#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>

int main( int argc, char *argv[] ) {
	pid_t 		pid;
	int		res;
	const char	*args[] = { "hello", "world", NULL };

	(void)argc;
	(void)argv;
	
	pid = fork();
	if( pid == 0 ) {
		printf( "Hi. I'm the child. My PID is: %d\n", getpid() );
		printf( "I'm about to call exec ....\n" );
		res = execv( "pt1", (char **)args );
		if( res ) {
			printf( "execv failed." );
			return -1;
		}
	}
	else {
		waitpid( pid, &res, 0 );
		printf( "Hi. I'm the parent. My child returned: %d\n", res );
	}

	return 0;
}
