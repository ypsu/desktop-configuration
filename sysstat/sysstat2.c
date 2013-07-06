#define _GNU_SOURCE
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <alsa/asoundlib.h>

// When the condition is true, output the source location and abort
#define HANDLE_CASE(cond) do{ if (cond) handle_case(#cond, __FILE__, __func__, __LINE__); } while(0)
static void handle_case(const char *expr, const char *file, const char *func, int line)
{
	printf("unhandled case, errno = %d (%m)\n", errno);
	printf("in expression '%s'\n", expr);
	printf("in function %s\n", func);
	printf("in file %s\n", file);
	printf("at line %d\n", line);
	abort();
}

enum colorflags {
	CLRFLG_DARK = 1,
	CLRFLG_BLUE = 2,
	CLRFLG_YELLOW = 4,
	CLRFLG_RED = 8,
};

struct state {
	// The timepoint when this state was acquired
	time_t time;
	struct timeval exact_time;

	// Memory in KiB
	int mem_dirty;
	int mem_writeback;
	int mem_committed;

	// The data transferred in bytes
	int net_down;
	int net_up;

	// Seconds spent
	double cpu_up;
	double cpu_idle;

	enum colorflags colorflags;
};

// If a file contains only one positive number, this extracts that
static long long extract_file_number(int fd)
{
	long long val = 0;
	static char buf[32];

	HANDLE_CASE(pread(fd, buf, 31, 0) <= 0);
	sscanf(buf, "%lld", &val);

	return val;
}

static void get_memory(struct state *s)
{
	static int fd = -1;
	if (fd == -1) {
		fd = open("/proc/meminfo", O_RDONLY);
		HANDLE_CASE(fd == -1);
	}
	char buf[4096];
	HANDLE_CASE(pread(fd, buf, 4096, 0) < 500);
	const int line_length = 28;
	long long committed;
	long long dirty;
	long long writeback;
	HANDLE_CASE(sscanf(buf+15*line_length, "Dirty: %lld", &dirty) != 1);
	HANDLE_CASE(sscanf(buf+16*line_length, "Writeback: %lld", &writeback) != 1);
	HANDLE_CASE(sscanf(buf+29*line_length, "Committed_AS: %lld", &committed) != 1);
	s->mem_dirty = dirty;
	s->mem_writeback = writeback;
	s->mem_committed = committed / 1024;
}

static void get_cpu(struct state *s)
{
	static int fd = -1;
	int rby;
	char buf[4096];

	if (fd == -1) {
		fd = open("/proc/uptime", O_RDONLY);
		HANDLE_CASE(fd == -1);
	}

	HANDLE_CASE((rby = pread(fd, buf, 4096, 0)) < 4);

	double up, idle;
	HANDLE_CASE(sscanf(buf, "%lf %lf", &up, &idle) != 2);

	s->cpu_up = up;
	s->cpu_idle = idle;
}

static void get_network(struct state *s)
{
	static int initialized = 0;
	enum { NETWORK_INTERFACE_CNT = 1 };
	static const char NETWORK_INTERFACE[1][8] = { "eth0" };
	static char up_fd[NETWORK_INTERFACE_CNT];
	static char down_fd[NETWORK_INTERFACE_CNT];
	int cur_int;

	if (initialized == 0) {
		initialized = 1;

		for (cur_int = 0; cur_int < NETWORK_INTERFACE_CNT; ++cur_int) {
			char buf[256];
			const char *iface = NETWORK_INTERFACE[cur_int];
			snprintf(buf, 256, "/sys/class/net/%s/statistics/rx_bytes", iface);
			down_fd[cur_int] = open(buf, O_RDONLY);

			snprintf(buf, 256, "/sys/class/net/%s/statistics/tx_bytes", iface);
			up_fd[cur_int] = open(buf, O_RDONLY);
		}
	}

	// Query the network information.
	for (cur_int = 0; cur_int < NETWORK_INTERFACE_CNT; ++cur_int) {
		s->net_down = extract_file_number(down_fd[cur_int]);
		s->net_up = extract_file_number(up_fd[cur_int]);
	}
}

static void get_time(struct state *s)
{
	s->time = time(NULL);
	gettimeofday(&s->exact_time, NULL);
}

static void get_colorflags(struct state *s)
{
	struct stat buf;

	s->colorflags = 0;
	if (stat("/dev/shm/D", &buf) == 0)
		s->colorflags |= CLRFLG_DARK;
	if (stat("/dev/shm/B", &buf) == 0)
		s->colorflags |= CLRFLG_BLUE;
	if (stat("/dev/shm/Y", &buf) == 0)
		s->colorflags |= CLRFLG_YELLOW;
	if (stat("/dev/shm/R", &buf) == 0)
		s->colorflags |= CLRFLG_RED;
}

static void get_state(struct state *s)
{
	get_time(s);
	get_memory(s);
	get_cpu(s);
	get_network(s);
	get_colorflags(s);
}

static int fd_state_file;

static void open_state_file(void)
{
	const char fname[] = "/tmp/.sysstat_state";
	fd_state_file = open(fname, O_RDWR);
	if (fd_state_file == -1 && errno == ENOENT) {
		fd_state_file = open(fname, O_RDWR | O_CREAT, 0600);
		HANDLE_CASE(fd_state_file == -1);
		static struct state s;
		int sz = sizeof s;
		HANDLE_CASE(pwrite(fd_state_file, &s, sz, 0) != sz);
	}
	HANDLE_CASE(fd_state_file == -1);
}

static void print_stats(const struct state *old, const struct state *new)
{
	struct tm *tm = localtime(&new->time);
	struct timeval td;
	timersub(&new->exact_time, &old->exact_time, &td);
	double tdiff = td.tv_sec + td.tv_usec/1000000.0;

	double cpudiff_up = new->cpu_up - old->cpu_up;
	double cpudiff_idle = new->cpu_idle - old->cpu_idle;
	int cpu_usage = lrint(100.0 - 100.0*cpudiff_idle / cpudiff_up);

	int net_up = lrint((new->net_up - old->net_up) / tdiff / 1000.0);
	int net_down = lrint((new->net_down - old->net_down) / tdiff / 1000.0);

	const char fg_yellow[] = "#[fg=brightyellow]";
	const char fg_white[] = "#[fg=white]";
	const char bg_dark[] = "#[bg=black]";
	const char bg_blue[] = "#[bg=blue]";
	const char bg_yellow[] = "#[bg=yellow]";
	const char bg_red[] = "#[bg=red]";
	const char dflt[] = "#[fg=default,bg=default]";

	printf("%s....%s %s....%s %s....%s %s....%s DRT:%s%7d%s KiB  MEM:%s%4d%s MiB  eth0:%s%5d%s/%s%5d%s KB  CPU:%s%3d%%  %02d:%02d%s\n",
			(new->colorflags & CLRFLG_RED) ? bg_red : dflt,
			dflt,
			(new->colorflags & CLRFLG_YELLOW) ? bg_yellow : dflt,
			dflt,
			(new->colorflags & CLRFLG_BLUE) ? bg_blue : dflt,
			dflt,
			(new->colorflags & CLRFLG_DARK) ? bg_dark : dflt,
			dflt,
			new->mem_writeback > 0 ? fg_yellow : fg_white,
			new->mem_dirty + new->mem_writeback,
			dflt,
			fg_white,
			new->mem_committed,
			dflt,
			fg_white,
			net_up,
			dflt,
			fg_white,
			net_down,
			dflt,
			fg_white,
			cpu_usage,
			tm->tm_hour,
			tm->tm_min,
			dflt);
}

int main(void)
{
	open_state_file();
	struct state old, new;
	int sz = sizeof(struct state);

	HANDLE_CASE(pread(fd_state_file, &old, sz, 0) != sz);

	get_state(&new);
	print_stats(&old, &new);

	HANDLE_CASE(pwrite(fd_state_file, &new, sz, 0) != sz);

	return 0;
}
