#ifndef SYSSTAT_H
#define SYSSTAT_H

#include <sys/sysinfo.h>

// The g_whatever_cur indexes point to the actual data.
// Just before it is the previous data, the history is a
// circular buffer.

// The time interval between two measurements in milliseconds.
#define TIME_GRANULARITY 1000

// IO statistics. {{{1
#define IO_CNT 1

struct IO_INFO {
	char name[8];
	long sectors_read;
	long sectors_written;
	int sectors_read_delta;
	int sectors_written_delta;
};

extern struct IO_INFO g_io[IO_CNT];

void update_io();

// Memory statistics. {{{1
// Committed memory in MB.
extern int g_memory_committed;

void update_memory();

struct CPU_INFO {
	unsigned user;   // user + nice
	unsigned system; // system + iowait
	unsigned idle;   // idle

	int running, blocked;
};

extern struct CPU_INFO g_cpu;

void update_cpu();

// Network statistics. {{{1
#define NETWORK_INTERFACE_CNT 1

extern const char NETWORK_INTERFACE[NETWORK_INTERFACE_CNT][16];

struct NETWORK_INFO {
	unsigned long long download;
	unsigned long long upload;
	unsigned long long down_delta;
	unsigned long long up_delta;
};

extern struct NETWORK_INFO g_network[NETWORK_INTERFACE_CNT];

void update_network();

// ACPI information. {{{1
struct ACPI_INFO {
	int bat_percentage;
	int temperature; /* celsius is the temperature unit */
	char bat_note[256];
};

extern struct ACPI_INFO g_acpi_info;

void update_acpi();

// Disk information. {{{1
// The disk size / free space information is in MB.
#define DISK_DEVICE_CNT 3

extern const char DISK_DEVICES[DISK_DEVICE_CNT][16];
extern const int MIN_SPACE[DISK_DEVICE_CNT];

struct DISK_INFO {
	unsigned long all;
	unsigned long free;
};

extern struct DISK_INFO g_disk_info[DISK_DEVICE_CNT];

void update_disk();
// }}}1
// Time and date information {{{1
#define DATE_LENGTH 5
extern char g_date[DATE_LENGTH+1];

void update_date();

// Volume information {{{1
extern int g_volume_percent;

void update_volume();

#endif // ifndef SYSSTAT_H
