#!/bin/bash

function logexec() {
	echo "$@"
	"$@"
}

function log() {
	echo -e "\e[33m""$@""\e[0m"
}

setfont -f /usr/share/kbd/consolefonts/ter-g12n.psf.gz
hostname eper

log Mounting system dirs
logexec mount -t proc proc /proc -o nosuid,noexec,nodev
logexec mount -t sysfs sys /sys -o nosuid,noexec,nodev
logexec mount -t tmpfs run /run -o mode=0755,nosuid,nodev
logexec mkdir -p /dev/pts /dev/shm
logexec mount -t devpts devpts /dev/pts -o mode=0620,gid=5,nosuid,noexec
logexec mount -t tmpfs shm /dev/shm -o mode=1777,nosuid,nodev
logexec mount -t tmpfs tmpfs /tmp -o nosuid,nodev,size=128000k

log Waiting for udev
logexec /usr/lib/systemd/systemd-udevd --daemon
logexec udevadm trigger --action=add --type=subsystems
logexec udevadm trigger --action=add --type=devices
logexec udevadm settle

log Bringing up networking
logexec ifconfig lo up
logexec ifconfig eth0 up
logexec ifconfig eth0 192.168.1.123 netmask 255.255.255.0 broadcast 192.168.1.255
logexec route add default gw 192.168.1.1 eth0

log Checking filesystems
logexec fsck -T -C /
if test $? -ge 2; then
	agetty -8 -a root --noclear 38400 tty1 linux
fi

log Mounting filesystems
logexec mount -o remount,rw /
logexec mount /boot

echo Setting up environment
export LC_ALL=en_US.UTF-8
export PATH=/root/.sbin:$PATH
logexec modprobe snd_bcm2835
logexec alsactl restore

log Miscellaneous settings
echo Setting tty keymap
(dumpkeys | grep -i keymaps; echo keycode 58 = Escape) | loadkeys - >/dev/null

echo Setting kernel variables
echo 100 > /proc/sys/vm/dirty_background_ratio
echo 100 > /proc/sys/vm/dirty_ratio
echo 30000 > /proc/sys/vm/dirty_expire_centisecs
echo 18000 > /proc/sys/vm/dirty_writeback_centisecs
echo 16384 > /proc/sys/vm/min_free_kbytes

log Starting X in the background
echo 'startx &'
cd ~rlblaster
PATH=/home/rlblaster/.bin:$PATH su -c "startx -- vt7 -nolisten tcp" rlblaster 2>/dev/null >&2 &
cd - >/dev/null

log Setting time
sntp

log Starting ttys
agetty -8 -a rlblaster -o "-p -f rlblaster" --noclear 38400 tty2 linux &
agetty -8 -a rlblaster -o "-p -f rlblaster" --noclear 38400 tty3 linux &
agetty -8 -a rlblaster -o "-p -f rlblaster" --noclear 38400 tty4 linux &
agetty -8 -a rlblaster -o "-p -f rlblaster" --noclear 38400 tty5 linux &
agetty -8 -a rlblaster -o "-p -f rlblaster" --noclear 38400 tty6 linux &
agetty -8 -a rlblaster -o "-p -f rlblaster" --noclear 38400 tty1 linux &
