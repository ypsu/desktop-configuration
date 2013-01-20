#define _GNU_SOURCE
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
	int chpid = fork();
	if (chpid == -1)
		abort();

	if (chpid == 0) {
		const char *argv[] = { "/root/boot.sh", NULL };
		const char *envp[] = { NULL };
		execve(argv[0], (char**) argv, (char**) envp);
		abort();
	} else {
		for (int sig = 0; sig < 32; ++sig)
			signal(sig, SIG_IGN);

		while (true) {
			wait(NULL);
		}
	}

	return 0;
}
