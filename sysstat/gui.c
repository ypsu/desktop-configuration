#include <assert.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <X11/extensions/Xdbe.h>

#include "sysstat.h"

#define TOPWIDTH 700
#define WIDTH 1920
#define HEIGHT 1080

#define WINDOW_HEIGHT 21

#define COL_BACKGROUND 0x000000
#define COL_LIGHT_BG 0x111111
#define COL_RED 0xFF0000
#define COL_MIDGROUND  0x888888
#define COL_FOREGROUND 0xCCCCCC

#define POS_TIME (TOPWIDTH - (DATE_LENGTH*g_width))
#define POS_CPU (POS_TIME - ((9+9)*g_width))
#define POS_VOL (POS_CPU - (9*g_width))
#define POS_NET (POS_VOL - ((6+4+1+4+1)*g_width))
#define POS_MEM (POS_NET - (13*g_width))
#define POS_BAT (POS_MEM - (9*g_width))
#define POS_YELLOW (POS_BAT - (2*g_width))
#define POS_BLUE (POS_YELLOW - 1*g_width)
#define POS_GREEN (POS_BLUE - 1*g_width)
#define POS_RED (POS_GREEN - 1*g_width)

// The draw_* functions assume that the window portion is already clean, and
// they don't flush the commands.

// Global variables {{{1
Display* g_dpy;
int g_screen;
Visual* g_visual;
Colormap g_colormap;
XftDraw *g_xftdraw;
Window g_topwindow, g_fullwindow;
XdbeBackBuffer g_backbuffer;
GC g_topgc, g_fullgc;
unsigned long g_black, g_white;
int g_height, g_width; // Font width and height.
int g_topbase, g_botbase; // Font baselines.
Font g_font;
XftFont* g_xftfont;
XftColor g_col_bg, g_col_fg, g_col_light, g_col_red, g_col_mid;

int g_cur_network_interface = 0;

void create_color(unsigned long color, XftColor *xftcolor) // {{{1
{
	char buf[32];
	sprintf(buf, "#%06lx", color);
	XftColorAllocName(g_dpy, g_visual, g_colormap, buf, xftcolor);
}

void setup_font(void) // {{{1
{
	const char fontstr[] = "-xos4-terminus-medium-r-normal--16-160-72-72-c-80-iso10646-1";
	g_xftfont = XftFontOpenXlfd(g_dpy, g_screen, fontstr);
	if(!g_xftfont)
		g_xftfont = XftFontOpenName(g_dpy, g_screen, fontstr);
	if(!g_xftfont) {
		fprintf(stderr, "error, cannot load font: '%s'\n", fontstr);
		exit(1);
	}
	XGlyphInfo *extents = malloc(sizeof(XGlyphInfo));
	XftTextExtentsUtf8(g_dpy, g_xftfont, (FcChar8*) "%", 1, extents);

	g_width = extents->width + 1;
	g_height = g_xftfont->ascent + g_xftfont->descent;
	assert(g_height == WINDOW_HEIGHT-5);
	g_topbase = g_xftfont->ascent + 1;
	g_botbase = HEIGHT - 1 - g_xftfont->descent;

	g_xftdraw = XftDrawCreate(g_dpy, g_backbuffer, g_visual, g_colormap);
	assert(g_xftdraw != NULL);

	create_color(COL_BACKGROUND, &g_col_bg);
	create_color(COL_FOREGROUND, &g_col_fg);
	create_color(COL_MIDGROUND, &g_col_mid);
	create_color(COL_LIGHT_BG, &g_col_light);
	create_color(COL_RED, &g_col_red);
}

void setup_xdbe(void) // {{{1
{
	int major, minor;

	if (!XdbeQueryExtension(g_dpy, &major, &minor)) {
		fprintf(stderr, "XdbeQueryExtension returned zero!\n");
		exit(3);
	}
	g_backbuffer = XdbeAllocateBackBufferName(g_dpy,
			g_topwindow, XdbeBackground);
	if (g_backbuffer == None) {
		fprintf(stderr, "Couldn't allocate a back buffer!\n");
		exit(3);
	}
}

void initialize(void) // {{{1
{
	XSetWindowAttributes wa;

	g_dpy = XOpenDisplay(0);
	assert(g_dpy != 0);

	g_screen = DefaultScreen(g_dpy);
	g_visual = DefaultVisual(g_dpy, g_screen);
	g_colormap = DefaultColormap(g_dpy, g_screen);

	g_black = BlackPixel(g_dpy, g_screen);
	g_white = WhitePixel(g_dpy, g_screen);

	memset(&wa, 0, sizeof wa);
	wa.override_redirect = 1;
	wa.background_pixmap = (Pixmap) ParentRelative;
	wa.event_mask = ExposureMask | ButtonPressMask | StructureNotifyMask;

	sleep(1);
	FILE *f = popen("xwininfo -name 'i3bar for output xroot-0' | grep 'Window id' | sed 's/.*: 0x\\([0-9a-f]*\\) .*/\\1/'", "r");
	assert(f != NULL);
	unsigned int parent;
	fscanf(f, "%x", &parent);
	fclose(f);
	//g_topwindow = XCreateWindow(g_dpy, DefaultRootWindow(g_dpy),
	g_topwindow = XCreateWindow(g_dpy, parent,
			WIDTH-TOPWIDTH, -1, TOPWIDTH, WINDOW_HEIGHT, 0, DefaultDepth(g_dpy, DefaultScreen(g_dpy)),
			CopyFromParent, DefaultVisual(g_dpy, DefaultScreen(g_dpy)),
			(unsigned long) (CWOverrideRedirect | CWBackPixmap | CWEventMask),
			&wa);

	XMapWindow(g_dpy, g_topwindow);

	setup_xdbe();
	setup_font();
	g_topgc = XCreateGC(g_dpy, g_backbuffer, 0, 0);
}

void draw_at_xy(int x, int y, XftColor *color, XftDraw *draw, const char *str, int lenmod) // {{{1
{
	XftDrawStringUtf8(draw, color, g_xftfont, x, y+2, (FcChar8*)str, strlen(str) - lenmod);
}

void draw_at_x(int x, XftColor *color, const char *str) // {{{1
{
	draw_at_xy(x, g_topbase, color, g_xftdraw, str, 0);
}

void toggle_big_screen(void) // {{{1
{
	XSetWindowAttributes wa;

	if (g_fullwindow != 0) {
		XDestroyWindow(g_dpy, g_fullwindow);
		g_fullwindow = 0;

		system("kill -9 `ps x | grep 'sleep 123.45' | grep -v grep | sed 's/ *\\([0-9]*\\).*/\\1/'`");
		return;
	}

	memset(&wa, 0, sizeof wa);
	wa.override_redirect = 1;
	wa.background_pixmap = (Pixmap) ParentRelative;
	wa.event_mask = ExposureMask | ButtonPressMask | StructureNotifyMask;

	g_fullwindow = XCreateWindow(g_dpy, DefaultRootWindow(g_dpy),
			0, WINDOW_HEIGHT, WIDTH, HEIGHT-WINDOW_HEIGHT, 0, DefaultDepth(g_dpy, DefaultScreen(g_dpy)),
			CopyFromParent, DefaultVisual(g_dpy, DefaultScreen(g_dpy)),
			(unsigned long) (CWOverrideRedirect | CWBackPixmap | CWEventMask),
			&wa);

	XMapWindow(g_dpy, g_fullwindow);
	XftDraw *xftdraw = XftDrawCreate(g_dpy, g_fullwindow, g_visual, g_colormap);

	g_fullgc = XCreateGC(g_dpy, g_fullwindow, 0, 0);

	for (;;) {
		XEvent e;
		XNextEvent(g_dpy, &e);
		if (e.type == MapNotify)
			break;
	}

	XSetForeground(g_dpy, g_fullgc, 0x000000);
	XFillRectangle(g_dpy, g_fullwindow, g_fullgc, 0, 0, WIDTH, HEIGHT-WINDOW_HEIGHT);

	// draw the calendar
	{
		FILE *f = popen("remind -c+5 -b1 -m -w238 ~/.reminders", "r");
		char buf[1024];
		int y;
		int i;

		for (y = 0; fgets(buf, 1024, f) != NULL; y += g_height) {
			draw_at_xy(0, y + g_topbase, &g_col_fg, xftdraw, buf, 1);
		}
		pclose(f);

		y += g_height;
		for (i = 0; i < NETWORK_INTERFACE_CNT; ++i) {
			sprintf(buf, "%5s: Down: %6lld MB; Up: %6lld MB", NETWORK_INTERFACE[i],
					g_network[i].download / 1024 / 1024, g_network[i].upload / 1024 / 1024);
			draw_at_xy(0, y + g_topbase, &g_col_fg, xftdraw, buf, 1);
			y += g_height;
		}

		y += g_height;
		sprintf(buf, "ACPI: %s", g_acpi_info.bat_note);
		draw_at_xy(0, y + g_topbase, &g_col_fg, xftdraw, buf, 0);
		y += g_height;
		sprintf(buf, "Temperature: %d C", g_acpi_info.temperature);
		draw_at_xy(0, y + g_topbase, &g_col_fg, xftdraw, buf, 0);
		y += g_height;

		f = popen("df -lh", "r");
		for (y += g_height; fgets(buf, 256, f) != NULL; y += g_height) {
			draw_at_xy(0, y + g_topbase, &g_col_fg, xftdraw, buf, 1);
		}
		pclose(f);
	}

	XftDrawDestroy(xftdraw);
	XFlush(g_dpy);
}

void update_statistics(void) // Updates the statistics data. Should be called in exactly in one second intervals.{{{1
{
	update_memory();
	update_cpu();
	update_network();
	//update_acpi();
	update_date();
	update_volume();
}

void draw_sound(void) // Draws the sound volume {{{1
{
	char buf[16];

	sprintf(buf, "VOL:");
	draw_at_x(POS_VOL, &g_col_mid, buf);

	sprintf(buf, "    %3d%%", g_volume_percent);
	draw_at_x(POS_VOL, &g_col_fg, buf);
}

void draw_cpu(void) // Draws the cpu bars {{{1
{
	char buf[32];

	unsigned user, system, idle;
	unsigned percentage;
	user = g_cpu.user;
	system = g_cpu.system;
	idle = g_cpu.idle;
	if (user+system+idle != 0)
		percentage = 100*(user+system) / (user+system+idle);
	else
		percentage = 0;

	sprintf(buf, "R/B:      CPU:");
	draw_at_x(POS_CPU, &g_col_mid, buf);

	sprintf(buf, "              %3d%%", percentage);
	draw_at_x(POS_CPU, (percentage<80) ? &g_col_fg : &g_col_red, buf);

	char r = '+', b = '+';
	if (g_cpu.running < 10)
		r = g_cpu.running + '0';
	if (g_cpu.blocked < 10)
		b = g_cpu.blocked + '0';
	sprintf(buf, "     %c/%c", r, b);
	draw_at_x(POS_CPU, &g_col_fg, buf);
}

void draw_memory(void) // Draws the memory usage {{{1
{
	char buf[32];

	sprintf(buf, "MEM:");
	draw_at_x(POS_MEM, &g_col_mid, buf);

	sprintf(buf, "    %5d MB", g_memory_committed);
	draw_at_x(POS_MEM, &g_col_fg, buf);
}

void draw_network(void) // Draws the network stats {{{1
{
	struct NETWORK_INFO *ci = &g_network[g_cur_network_interface];

#define AGGREGATE_SIZE 4

	static unsigned long long ups[AGGREGATE_SIZE], downs[AGGREGATE_SIZE];
	static int cur_index;
	static unsigned long long up_sum, down_sum;

	up_sum -= ups[cur_index];
	down_sum -= downs[cur_index];

	ups[cur_index] = ci->up_delta;
	downs[cur_index] = ci->down_delta;

	up_sum += ups[cur_index];
	down_sum += downs[cur_index];

	cur_index = (cur_index + 1) % AGGREGATE_SIZE;

	char buf[64];

	sprintf(buf, "%5s:", NETWORK_INTERFACE[g_cur_network_interface]);
	draw_at_x(POS_NET, &g_col_mid, buf);

	sprintf(buf, "      %4llu/%4llu", down_sum / AGGREGATE_SIZE / 1024 / (TIME_GRANULARITY/1000), up_sum / AGGREGATE_SIZE / 1024 / (TIME_GRANULARITY/1000));
	draw_at_x(POS_NET, &g_col_fg, buf);
}

void draw_time(void) // Draws the current time {{{1
{
	draw_at_x(POS_TIME, &g_col_mid, g_date);
}

void draw_bat(void) // Draws the battery status {{{1
{
	char buf[16];

	if (g_acpi_info.bat_percentage > 90 || g_acpi_info.bat_percentage == 0)
		return;

	sprintf(buf, "BAT:");
	draw_at_x(POS_BAT, &g_col_mid, buf);

	sprintf(buf, "    %3d%%", g_acpi_info.bat_percentage);
	draw_at_x(POS_BAT, (g_acpi_info.bat_percentage>=20) ? &g_col_fg : &g_col_red, buf);
}

void draw_indicators(void) // Draws the colored indicators {{{1
{
	struct stat buf;

	if (stat("/dev/shm/G", &buf) == 0) {
		XSetForeground(g_dpy, g_topgc, 0x00FF00);
		XFillRectangle(g_dpy, g_backbuffer, g_topgc, POS_GREEN, 0, 1*g_width, WINDOW_HEIGHT-1);
	}

	if (stat("/dev/shm/B", &buf) == 0) {
		XSetForeground(g_dpy, g_topgc, 0x0000FF);
		XFillRectangle(g_dpy, g_backbuffer, g_topgc, POS_BLUE, 0, 1*g_width, WINDOW_HEIGHT-1);
	}

	if (stat("/dev/shm/Y", &buf) == 0) {
		XSetForeground(g_dpy, g_topgc, 0xFFFF00);
		XFillRectangle(g_dpy, g_backbuffer, g_topgc, POS_YELLOW, 0, 1*g_width, WINDOW_HEIGHT-1);
	}

	if (stat("/dev/shm/R", &buf) == 0) {
		XSetForeground(g_dpy, g_topgc, 0xFF0000);
		XFillRectangle(g_dpy, g_backbuffer, g_topgc, POS_RED, 0, 1*g_width, WINDOW_HEIGHT-1);
	}
}

void swap_buffers(void) // {{{1
{
	XdbeSwapInfo swap;

	swap.swap_window = g_topwindow;
	swap.swap_action = XdbeBackground;
	XdbeSwapBuffers(g_dpy, &swap, 1);
}

void update_screen(void) // Updates the screen {{{1
{
	//XSetForeground(g_dpy, g_topgc, 0x004000);
	//XFillRectangle(g_dpy, g_backbuffer, g_topgc, 0, 0, WIDTH, WINDOW_HEIGHT);

	draw_sound();
	draw_cpu();
	draw_memory();
	draw_network();
	draw_time();
	draw_bat();
	draw_indicators();

	swap_buffers();
	XFlush(g_dpy);
}

int main(void) // {{{1
{
	int con_num;
	fd_set in_fds;
	struct timeval nextupdate;
	struct timeval update_interval;
	struct timeval curtime;

	update_interval.tv_sec = TIME_GRANULARITY / 1000;
	update_interval.tv_usec = TIME_GRANULARITY % 1000 * 1000;

	initialize();
	con_num = ConnectionNumber(g_dpy);

	// Wait for the MapNotify event.
	for (;;) {
		XEvent e;
		XNextEvent(g_dpy, &e);
		if (e.type == MapNotify)
			break;
	}

	//XDrawLine(g_dpy, g_topwindow, g_topgc, 10, 60, 180, 20);
	//XDrawString(g_dpy, g_topwindow, g_topgc, 50, 50, str, strlen(str));
	//XSetForeground(g_dpy, g_topgc, 0x0000FF);
	//XFillRectangle(g_dpy, g_topwindow, g_topgc, 0, 0, TOPWIDTH, WINDOW_HEIGHT);

	update_statistics();
	update_statistics();
	update_volume();
	gettimeofday(&curtime, 0);
	timeradd(&curtime, &update_interval, &nextupdate);
	update_screen();
	XFlush(g_dpy);

	// The main loop.
	for (;;) {
		struct timeval waittime;
		XEvent e;

		timersub(&nextupdate, &curtime, &waittime);

		FD_ZERO(&in_fds);
		FD_SET(con_num, &in_fds);
		select(con_num+1, &in_fds, 0, 0, &waittime);

		while (XPending(g_dpy) != 0) {
			XNextEvent(g_dpy, &e);
			if (e.type == ButtonPress) {
				switch (e.xbutton.button) {
				case 1:
					toggle_big_screen();
					break;

				case 3:
					g_cur_network_interface = 1 - g_cur_network_interface;
					break;

				case 4:
					system("amixer -M sset PCM 5%+ >/dev/null");
					update_volume();
					break;

				case 5:
					system("amixer -M set PCM 5%- >/dev/null");
					update_volume();
					break;

				/* default:
					printf("the button %u was pressed\n", e.xbutton.button);
					return 0; */
				}
			}
		}

		gettimeofday(&curtime, 0);

		if (timercmp(&nextupdate, &curtime, <)) {
			update_statistics();
			do {
				timeradd(&nextupdate, &update_interval, &nextupdate);
			} while (timercmp(&nextupdate, &curtime, <));
		}

		update_screen();
	}
	return 0;
}
