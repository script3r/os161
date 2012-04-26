#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>

int main( int argc, char *argv[] ) {
	pid_t		pid = -1;
	pid_t		mypid = -1;

	(void)argc;
	(void)argv;
	
	mypid = getpid();
	pid = fork();
	if( pid == 0 ) {
		mypid = getpid();
		printf( "I'm the child." );
	} 
	else {
		printf( "I'm the parent. %d\n", mypid );
	}

	return 0;
}
