#define _GNU_SOURCE
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <alsa/asoundlib.h>

#include "sysstat.h"

// When the condition is true, output the source location and abort
#define HANDLE_CASE(cond) do{ if (cond) handle_case(#cond, __FILE__, __func__, __LINE__); } while(0)
void handle_case(const char *expr, const char *file, const char *func, int line)
{
	printf("unhandled case, errno = %d (%m)\n", errno);
	printf("in expression '%s'\n", expr);
	printf("in function %s\n", func);
	printf("in file %s\n", file);
	printf("at line %d\n", line);
	abort();
}

long long extract_file_number(int fd) // If a file contains only one positive number, this extracts that. {{{1
{
	long long val = 0;
	static char buf[32];

	HANDLE_CASE(pread(fd, buf, 31, 0) <= 0);
	sscanf(buf, "%lld", &val);

	return val;
}
// Global variable definitions. {{{1
struct IO_INFO g_io[IO_CNT];

int g_memory_committed;

struct CPU_INFO g_cpu;

const char NETWORK_INTERFACE[NETWORK_INTERFACE_CNT][16] = { "eth0" };
struct NETWORK_INFO g_network[NETWORK_INTERFACE_CNT];

struct ACPI_INFO g_acpi_info;

const char DISK_DEVICES[DISK_DEVICE_CNT][16] = { "/", "/data", "/home" };
const int MIN_SPACE[DISK_DEVICE_CNT] = { 2, 10, 2 };
struct DISK_INFO g_disk_info[DISK_DEVICE_CNT];

char g_date[DATE_LENGTH+1];

int g_volume_percent;

void update_io() // {{{
{
	static int fd = -1;
	if (fd == -1) {
		strcpy(g_io[0].name, "sda");
		fd = open("/proc/diskstats", O_RDONLY);
		HANDLE_CASE(fd == -1);
	}
	char buf[4096];
	HANDLE_CASE(pread(fd, buf, 4096, 0) < 500);
	char *p = memchr(buf, 'a', 4096);
	HANDLE_CASE(p == NULL);
	p += 1;
	long long rd, wn;
	HANDLE_CASE(sscanf(p, "%*s %*s %lld %*s %*s %*s %lld", &rd, &wn) != 2);
	g_io[0].sectors_read_delta = rd - g_io[0].sectors_read;
	g_io[0].sectors_written_delta = wn - g_io[0].sectors_written;
	g_io[0].sectors_read = rd;
	g_io[0].sectors_written = wn;
}

void update_memory() // {{{1
{
	static int fd = -1;
	if (fd == -1) {
		fd = open("/proc/meminfo", O_RDONLY);
		HANDLE_CASE(fd == -1);
	}
	char buf[4096];
	HANDLE_CASE(pread(fd, buf, 4096, 0) < 500);
	long long committed;
	HANDLE_CASE(sscanf(buf+812, "Committed_AS: %lld", &committed) != 1);
	g_memory_committed = committed / 1024;
}

void update_cpu() // {{{1
{
	static int fd = -1;
	static struct CPU_INFO saved;
	int rby;
	char buf[4096];

	if (fd == -1) {
		fd = open("/proc/stat", O_RDONLY);
		HANDLE_CASE(fd == -1);
	}

	HANDLE_CASE((rby = pread(fd, buf, 4096, 0)) < 100);

	unsigned user, nice, system, idle, iowait;
	HANDLE_CASE(sscanf(buf, "cpu %u %u %u %u %u", &user, &nice, &system, &idle, &iowait) != 5);
	g_cpu.user = user + nice - saved.user;
	g_cpu.system = system + iowait - saved.system;
	g_cpu.idle = idle - saved.idle;

	saved.user = user + nice;
	saved.system = system + iowait;
	saved.idle = idle;

	char *p = memmem(buf, rby, "procs_running", 13);
	HANDLE_CASE(p == NULL);
	int running, blocked;
	HANDLE_CASE(sscanf(p, "procs_running %d procs_blocked %d", &running, &blocked) != 2);
	g_cpu.running = running-1;
	g_cpu.blocked = blocked;
}

void update_network() // {{{1
{
	static int initialized = 0;
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
		long long down, up;
		long long old_down, old_up;
		long long delta_down = 0, delta_up = 0;
		old_down = g_network[cur_int].download;
		old_up = g_network[cur_int].upload;
		down = extract_file_number(down_fd[cur_int]);
		up = extract_file_number(up_fd[cur_int]);

		if (old_down > 4000000000u && down < old_down)
			down += 1LL << 32;
		if (old_up > 4000000000u && up < old_up)
			up += 1LL << 32;

		if (down >= old_down)
			delta_down = down - old_down;
		if (up >= old_up)
			delta_up = up - old_up;

		g_network[cur_int].down_delta = delta_down;
		g_network[cur_int].up_delta = delta_up;
		g_network[cur_int].download = down;
		g_network[cur_int].upload = up;
	}
}

void update_acpi() // {{{1
{
	FILE* fbat;
	FILE* ftemp;
	FILE* fbat_note;

	static int call_cnt = 0;

	// Gather data only every 64 second.
	if (call_cnt++ % 64 != 0)
		return;

	fbat = popen("acpi -b | sed 's/^.* \\([0-9]*\\)%.*$/\\1/'", "r");
	ftemp = popen("acpi -t | sed 's/^.*:.* \\([0-9]*\\)\\..*$/\\1/'", "r");
	fbat_note = popen("acpi -b | sed 's/^.*: //'", "r");

	if (fbat != 0) {
		fscanf(fbat, "%d", &g_acpi_info.bat_percentage);
		pclose(fbat);
	}

	if (ftemp != 0) {
		fscanf(ftemp, "%d", &g_acpi_info.temperature);
		pclose(ftemp);
	}

	if (fbat_note != 0) {
		fscanf(fbat_note, "%[^\n]", g_acpi_info.bat_note);
		pclose(fbat_note);
	}
}

void update_disk() // {{{1
{
	int cur_disk;
	for (cur_disk = 0; cur_disk < DISK_DEVICE_CNT; ++cur_disk) {
		struct statvfs st;
		unsigned long k_bs; /* blocksize in kilobytes */
		statvfs(DISK_DEVICES[cur_disk], &st);

		k_bs = st.f_frsize / 1024;
		g_disk_info[cur_disk].all = st.f_blocks * k_bs / 1024;
		g_disk_info[cur_disk].all = st.f_bfree * k_bs / 1024;
	}
}

void update_date() // {{{1
{
	time_t tv = 0;
	struct tm* tm;
	time(&tv);
	tm = localtime(&tv);
	if (tm != 0) {
		snprintf(g_date, DATE_LENGTH+1, "%02d:%02d",
				tm->tm_hour, tm->tm_min);
	}
}

#define MAX_LINEAR_DB_SCALE 24

static inline bool use_linear_dB_scale(long dBmin, long dBmax)
{
	return dBmax - dBmin <= MAX_LINEAR_DB_SCALE * 100;
}

static double get_normalized_volume(snd_mixer_elem_t *elem, snd_mixer_selem_channel_id_t channel)
{
	long min, max, value;
	double normalized;
	//double min_norm;
	int err;

	err = snd_mixer_selem_get_playback_dB_range(elem, &min, &max);
	if (err < 0 || min >= max) {
		err = snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
		if (err < 0 || min == max)
			return 0;

		err = snd_mixer_selem_get_playback_volume(elem, channel, &value);
		if (err < 0)
			return 0;

		return (value - min) / (double)(max - min);
	}

	err = snd_mixer_selem_get_playback_dB(elem, channel, &value);
	if (err < 0)
		return 0;

	if (use_linear_dB_scale(min, max))
		return (value - min) / (double)(max - min);

	normalized = exp10((value - max) / 6000.0);
	//min_norm = exp10((min - max) / 6000.0);
	//normalized = (normalized - min_norm) / (1 - min_norm);

	return normalized;
}

void update_volume() // {{{1
{
	int err;
	bool first_run = false;
	static bool initialized = false;
	static snd_mixer_t *snd_mixer;
	static snd_mixer_elem_t *elem;
	static snd_mixer_selem_id_t *sid;

	if (!initialized) {
		initialized = true;
		first_run = true;
		err = snd_mixer_open(&snd_mixer, 0);
		if (err < 0) {
			puts(snd_strerror(err));
			exit(2);
		}

		err = snd_mixer_attach(snd_mixer, "default");
		if (err < 0) {
			puts(snd_strerror(err));
			exit(2);
		}
		err = snd_mixer_selem_register(snd_mixer, NULL, NULL);
		if (err < 0) {
			puts(snd_strerror(err));
			exit(2);
		}

		err = snd_mixer_load(snd_mixer);
		if (err < 0) {
			puts(snd_strerror(err));
			exit(2);
		}

		snd_mixer_selem_id_malloc(&sid);
		snd_mixer_selem_id_set_index(sid, 0);
		snd_mixer_selem_id_set_name(sid, "PCM");
		elem = snd_mixer_find_selem(snd_mixer, sid);
		if (elem == NULL) {
			puts("error in snd_mixer_find_selem");
			exit(2);
		}
	}

	err = snd_mixer_handle_events(snd_mixer);
	if (err < 0) {
		puts(snd_strerror(err));
		exit(2);
	}
	if (first_run || err > 0) {
		g_volume_percent = lrint(100.0*get_normalized_volume(elem, SND_MIXER_SCHN_MONO));
	}
}
