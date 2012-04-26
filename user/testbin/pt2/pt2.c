#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>

int main( int argc, char *argv[] ) {
	pid_t		pid;
	pid_t		mypid;

	(void)argc;
	(void)argv;
	
	pid = fork();
	if( pid == 0 ) {
		mypid = getpid();
		printf( "I'm the child with pid %d\n", mypid );
	}

	return 0;
}
