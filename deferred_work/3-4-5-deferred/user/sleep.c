#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	unsigned long seconds;
	seconds = atoi(argv[1]);
	printf("[sleep] PID=%d sleep for %ld seconds\n", (int) getpid(), seconds);
	sleep(seconds);
	printf("[sleep] PID=%d woke up, now exiting\n", (int) getpid());
	return 0;
}


