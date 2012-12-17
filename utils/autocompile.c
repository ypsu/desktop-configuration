#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <ftw.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>

#define HANDLE_CASE(cond) {if ((cond)) handle_case(#cond, __FILE__, __func__, __LINE__);}

void handle_case(const char *expr, const char *file, const char *func, int line)
{
	printf("unhandled case, errno = %d (%m)\n", errno);
	printf("in expression '%s'\n", expr);
	printf("in function %s\n", func);
	printf("in file %s\n", file);
	printf("at line %d\n", (int) line);
	exit(1);
}

bool just_make_it;
int inotify_fd;

int visit_dirs(const char *filename, const struct stat *stat, int flag)
{
	(void) stat;
	if (flag != FTW_D) return 0;
	if (strstr(filename, "/.git") != NULL) return 0;
	int rv = inotify_add_watch(inotify_fd, filename, IN_CLOSE_WRITE);
	HANDLE_CASE(rv == -1);
	return 0;
}

#define BUFSIZE 65536
char inotify_buffer[BUFSIZE];
int begin, size;

struct inotify_event* next_event(void)
{
	if (begin == size) {
		begin = 0;
		size = read(inotify_fd, inotify_buffer, BUFSIZE);
		HANDLE_CASE(size <= 0);
	}
	struct inotify_event *ev = (struct inotify_event*) &inotify_buffer[begin];
	begin += sizeof(*ev) + ev->len;
	return ev;
}

bool ends_with(const char *name, const char *suffix)
{
	size_t suffix_length = strlen(suffix);
	size_t name_length = strlen(name);
	if (suffix_length > name_length) return false;
	return strcmp(name+name_length-suffix_length, suffix) == 0;
}

bool cpp_source(const char *name)
{
	bool cpp_source = false;
	cpp_source = cpp_source || ends_with(name, ".cc");
	cpp_source = cpp_source || ends_with(name, ".cpp");
	cpp_source = cpp_source || ends_with(name, ".C");
	return cpp_source;
}

void exec_command(const char *cmd)
{
	printf("\e[H\e[2J");
	printf("\e[H\e[2J");
	printf("\e[H\e[2J");
	puts(cmd);
	int rv = system(cmd);
	if (rv == 0)
		printf("\e[0;32mdone\n");
	else
		printf("\e[0;31mdone\n");
	printf("\e[0m");
	fflush(stdout);
}

int main(void)
{
	struct stat stat_buf;
	int rv = stat("Makefile", &stat_buf);
	if (rv == 0) just_make_it = true;

	inotify_fd = inotify_init();
	HANDLE_CASE(inotify_fd == -1);
	ftw(".", visit_dirs, 128);
	while (true) {
		char cmdbuf[4000];
		bool should_make = false;
		do {
			struct inotify_event *ev;
			ev = next_event();
			HANDLE_CASE(ev->len > 3000);
			if (just_make_it) {
				should_make = should_make || ends_with(ev->name, ".tex");
				should_make = should_make || ends_with(ev->name, ".h");
				should_make = should_make || ends_with(ev->name, ".hh");
				should_make = should_make || ends_with(ev->name, ".hpp");
				should_make = should_make || ends_with(ev->name, ".c");
				should_make = should_make || cpp_source(ev->name);
				should_make = should_make || ends_with(ev->name, ".sh");
				should_make = should_make || ends_with(ev->name, "akefile");
			} else {
				if (ends_with(ev->name, ".c")) {
					sprintf(cmdbuf, "clang -mfloat-abi=hard -D__ARM_PCS_VFP "
							"-fstack-protector-all -std=c99 "
							"-lm -Wall -Wextra -g3 '%s' -lgmp "
							"-lpthread -lrt", ev->name);
					exec_command(cmdbuf);
				} else if (cpp_source(ev->name)) {
					sprintf(cmdbuf, "clang++ -fstack-protector-all "
							"-mfloat-abi=hard -D__ARM_PCS_VFP "
							"-D_GLIBCXX_DEBUG -DATHOME -lm -Wall "
							"-Wextra -g3 '%s' -lgmp -lpthread -lrt "
							"-L/home/rlblaster/.bin/ -lclangfix "
							"-Wl,-rpath,/home/rlblaster/.bin/ ",
							ev->name);
					exec_command(cmdbuf);
				}
			}
		} while (begin < size);
		if (should_make) {
			exec_command("make");
		}
	}
	return 0;
}
