// Small program which starts a new terminal in the same directory as the
// currently focused terminal is if any.
// It does this by going through all processes looking for topmost window's
// window ID found in the WINDOWID environment variable in the terminals.
// If nothing was found (e.g. the browser is the topmost window) the terminal
// starts in the home directory.

#define _GNU_SOURCE
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>

unsigned long get_focused_window(void)
{
	Window wid;
	int revert;
	Display *dpy;
	
	dpy = XOpenDisplay(NULL);
	if (dpy == NULL)
		return -1;
	XGetInputFocus(dpy, &wid, &revert);
	XCloseDisplay(dpy);
	return (unsigned long) wid;
}

int has_wid(const char *dirname, const char *wid)
{
	const char WID_ENV[] = "\0WINDOWID=";
	char fname[64];
	char environ[4096];
	size_t length;
	char *wid_start;
	FILE *f;

	sprintf(fname, "/proc/%s/environ", dirname);

	f = fopen(fname, "r");
	fseek(f, 0, SEEK_END);
	length = fread(environ, 1, sizeof(environ), f);
	fclose(f);

	wid_start = memmem(environ, length, WID_ENV, sizeof(WID_ENV) - 1);
	if (wid_start == NULL)
		return 0;
	wid_start += sizeof(WID_ENV) - 1;

	return strcmp(wid_start, wid) == 0;
}

int is_bash(const char *dirname)
{
	char fname[64];
	char exe[16] = {};
	sprintf(fname, "/proc/%s/exe", dirname);
	readlink(fname, exe, sizeof(exe) - 1);
	return strcmp(exe, "/bin/bash") == 0;
}

void chdir_to_current_bash(unsigned long wid)
{
	char wid_str[64];
	DIR *dirp;
	struct dirent *dirent;

	sprintf(wid_str, "%ld", wid);

	dirp = opendir("/proc");
	while ((dirent = readdir(dirp)) != NULL) {
		const char *dname = dirent->d_name;
		if (!isdigit(dname[0]))
			continue;
		if (is_bash(dname) && has_wid(dname, wid_str)) {
			char bash_path[64];
			sprintf(bash_path, "/proc/%s/cwd", dname);
			chdir(bash_path);
			return;
		}
	}
	closedir(dirp);
}

char cmd[] = "/usr/bin/xterm";
int main(int argc, char **argv)
{
	assert(argv[argc] == NULL);

	unsigned long wid = get_focused_window();
	chdir_to_current_bash(wid);
	argv[0] = cmd;
	execv(cmd, argv);
	return 0;
}
