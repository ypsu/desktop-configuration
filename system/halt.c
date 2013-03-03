#define _GNU_SOURCE
#include <ctype.h>
#include <dirent.h>
#include <linux/reboot.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/reboot.h>
#include <unistd.h>

int mypid;

// Returns true if there was no process available
bool killall(int sig)
{
	bool found_process = false;

	DIR *dir = opendir("/proc");
	if (dir == NULL) abort();

	struct dirent *dirent;
	while ((dirent = readdir(dir)) != NULL) {
		if (dirent->d_type != DT_DIR)
			continue;
		if (!isdigit(dirent->d_name[0]))
			continue;
		int pid = atoi(dirent->d_name);
		if (pid == 1 || pid == mypid)
			continue;
		char exe[64];
		sprintf(exe, "/proc/%d/exe", pid);
		char link[4096];
		if (readlink(exe, link, sizeof link) == -1)
			continue;
		kill(pid, sig);
		found_process = true;
	}

	closedir(dir);
	return !found_process;
}

int main(int argc, char **argv)
{
	if (argc < 1) {
		puts("????");
		exit(2);
	}

	enum { CMD_REBOOT, CMD_POWEROFF } request;
	if (strcmp(argv[0], "reboot") == 0) {
		request = CMD_REBOOT;
	} else if (strcmp(argv[0], "poweroff") == 0) {
		request = CMD_POWEROFF;
	} else {
		puts("Please use either \"reboot\" or \"poweroff\"!");
		exit(1);
	}

	if (geteuid() != 0) {
		puts("Must be root to execute this!");
		exit(1);
	}
	setuid(0);

	freopen("/dev/console", "w", stdout);
	freopen("/dev/console", "w", stderr);
	setlinebuf(stdout);
	setlinebuf(stderr);

	system("chvt 1");
	mypid = getpid();
	for (int sig = 0; sig < 32; ++sig) {
		signal(sig, SIG_IGN);
	}

	puts("Sending SIGTERM to the processes");
	for (int i = 0; i < 5; ++i) {
		if (killall(SIGTERM))
			break;
		sleep(1);
	}
	puts("Sending SIGKILL to the processes");
	for (int i = 0; i < 5; ++i) {
		if (killall(SIGKILL))
			break;
		sleep(1);
	}

	puts("Unmounting filesystems");
	system("umount -r -a");
	puts("Syncing");
	sync();
	puts("Remounting root");
	system("mount -r -o remount,ro /");
	sync();
	if (request == CMD_REBOOT) {
		puts("Rebooting");
		sleep(1);
		reboot(LINUX_REBOOT_CMD_RESTART);
	} else if (request == CMD_POWEROFF) {
		puts("Shutting down");
		sleep(1);
		reboot(LINUX_REBOOT_CMD_POWER_OFF);
	}
	return 0;
}
