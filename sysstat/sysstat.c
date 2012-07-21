#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <alsa/asoundlib.h>

#include "sysstat.h"

long long extract_file_number(const char* fname) // If a file contains only one positive number, this extracts that. {{{1
{
	FILE* f;
	long long val = 0;

	if (fname == 0)
		return 0;
	
	f = fopen(fname, "r");
	if (f != 0) {
		fscanf(f, "%lld", &val);
		fclose(f);
	}

	return val;
}
// Global variable definitions. {{{1
int g_memory_free;

struct CPU_INFO g_cpu[CPU_CNT];

const char NETWORK_INTERFACE[NETWORK_INTERFACE_CNT][16] = { "eth0", "eth1" };
struct NETWORK_INFO g_network[NETWORK_INTERFACE_CNT];

struct ACPI_INFO g_acpi_info;

const char DISK_DEVICES[DISK_DEVICE_CNT][16] = { "/", "/data", "/home" };
const int MIN_SPACE[DISK_DEVICE_CNT] = { 2, 10, 2 };
struct DISK_INFO g_disk_info[DISK_DEVICE_CNT];

char g_date[DATE_LENGTH+1];

int g_volume_percent;

void update_memory() // {{{1
{
	FILE *f_meminfo;
	char buf[256];
	int total = -1, anonPages = -1;

	f_meminfo = fopen("/proc/meminfo", "r");
	if (f_meminfo == 0) {
		printf("Fatal error: Couldn't open /proc/meminfo.\n");
		exit(EXIT_FAILURE);
	}

	while (fgets(buf, 256, f_meminfo) != NULL && (total == -1 || anonPages == -1)) {
		if (total == -1 && strncmp(buf, "MemTotal", 8) == 0) {
			sscanf(buf, "MemTotal: %d", &total);
		} else if (anonPages == -1 && strncmp(buf, "AnonPages", 9) == 0) {
			sscanf(buf, "AnonPages: %d", &anonPages);
		}
	}

	fclose(f_meminfo);

	if (total == -1 || anonPages == -1) {
		printf("Runtime error in memory stat collecting routine.\n");
		exit(EXIT_FAILURE);
	}

	g_memory_free = (anonPages*100 + 50) / total;
}

void update_cpu() // {{{1
{
	int cur_cpu;
	char buf[256];
	FILE* fstat;
	static struct CPU_INFO cpus[CPU_CNT];

	fstat = fopen("/proc/stat", "r");
	if (fstat == 0) {
		printf("Fatal error: Couldn't open /proc/stat.\n");
		exit(EXIT_FAILURE);
	}

	// Query the cpu information.
	for (cur_cpu = 0; cur_cpu < CPU_CNT; ++cur_cpu) {
		fgets(buf, 256, fstat);
		if (strncmp(buf, "cpu", 3) != 0) {
			cur_cpu -= 1;
			continue;
		}
		if (buf[3]-'0' == (char) cur_cpu) {
			char ch;
			unsigned user, nice, system, idle, iowait;
			sscanf(buf, "cpu%c %u %u %u %u %u",
					&ch, &user, &nice, &system, &idle, &iowait);

			g_cpu[cur_cpu].user = user + nice - cpus[cur_cpu].user;
			g_cpu[cur_cpu].system = system + iowait - cpus[cur_cpu].system;
			g_cpu[cur_cpu].idle = idle - cpus[cur_cpu].idle;

			cpus[cur_cpu].user = user + nice;
			cpus[cur_cpu].system = system + iowait;
			cpus[cur_cpu].idle = idle;
		} else {
			cur_cpu -= 1;
			continue;
		}
	}

	fclose(fstat);
}

void update_network() // {{{1
{
	static int initialized = 0;
	static char up_filename[NETWORK_INTERFACE_CNT][256];
	static char down_filename[NETWORK_INTERFACE_CNT][256];
	int cur_int;

	if (initialized == 0) {
		initialized = 1;

		for (cur_int = 0; cur_int < NETWORK_INTERFACE_CNT; ++cur_int) {
			snprintf(down_filename[cur_int], 256,
					"/sys/class/net/%s/statistics/rx_bytes",
					NETWORK_INTERFACE[cur_int]);
			snprintf(up_filename[cur_int], 256,
					"/sys/class/net/%s/statistics/tx_bytes",
					NETWORK_INTERFACE[cur_int]);
		}
	}

	// Query the network information.
	for (cur_int = 0; cur_int < NETWORK_INTERFACE_CNT; ++cur_int) {
		long long down, up;
		down = extract_file_number(down_filename[cur_int]);
		up = extract_file_number(up_filename[cur_int]);
		g_network[cur_int].down_delta = down - g_network[cur_int].download;
		g_network[cur_int].up_delta = up - g_network[cur_int].upload;
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

void update_volume() // {{{1
{
	int err;
	long int pmin, pmax, pvol;
	static bool initialized = false;
	static snd_mixer_t *snd_mixer;
	static snd_mixer_elem_t *elem;
	static snd_mixer_selem_id_t *sid;

	if (!initialized) {
		initialized = true;

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
		snd_mixer_selem_id_set_name(sid, "Master");
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
	err = snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_MONO, &pvol);
	if (err < 0) {
		puts(snd_strerror(err));
		exit(2);
	}

	snd_mixer_selem_get_playback_volume_range(elem, &pmin, &pmax);
	g_volume_percent = (int) (pvol*100 / pmax);
}
