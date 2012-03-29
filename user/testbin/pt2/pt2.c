#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>

int main( int argc, char *argv[] ) {
	pid_t 		pid;
	int		res;

	(void)argc;
	(void)argv;
	
	pid = fork();
	if( pid == 0 ) {
		printf( "Hi. I'm the child. My PID is: %d\n", getpid() );
	}
	else {
		waitpid( pid, &res, 0 );
		printf( "Hi. I'm the parent. My child returned: %d\n", res );
	}

	return 0;
}
