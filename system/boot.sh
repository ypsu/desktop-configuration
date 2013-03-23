#!/bin/bash

function logexec() {
	echo "$@"
	"$@"
}

function log() {
	echo -e "\e[33m""$@""\e[0m"
}

hostname shiro

log Mounting system dirs
logexec mount -t tmpfs run /run -o mode=0755,nosuid,nodev
logexec mkdir -p /dev/pts /dev/shm
logexec mount -t devpts devpts /dev/pts -o mode=0620,gid=5,nosuid,noexec
logexec mount -t tmpfs shm /dev/shm -o mode=1777,nosuid,nodev
logexec mount -t tmpfs tmpfs /tmp -o nosuid,nodev,size=2048000k

log Waiting for udev
logexec udevd --daemon
logexec udevadm trigger --action=add --type=subsystems
logexec udevadm trigger --action=add --type=devices
logexec udevadm settle

log Checking filesystems
fsck -T -C / && fsck -T -C /home && fsck -T -C /data
if test $? -ge 2; then
	agetty -8 -a root --noclear 38400 tty1 linux
fi

log Mounting filesystems
logexec mount -o remount,rw /
logexec mount /boot
logexec mount /home
logexec mount /data

echo Setting up environment
export LC_ALL=en_US.UTF-8
export PATH=/root/.sbin:$PATH

logexec alsactl restore
log Starting dbus and wicd
(install -m755 -g 81 -o 81 -d /run/dbus; dbus-daemon --system; /usr/sbin/wicd) &

log Miscellaneous settings
hdparm -B 255 /dev/sda >/dev/null
echo Setting tty keymap
(dumpkeys | grep -i keymaps; echo keycode 58 = Escape) | loadkeys - >/dev/null

echo Setting kernel variables
echo 100 > /proc/sys/vm/dirty_background_ratio
echo 100 > /proc/sys/vm/dirty_ratio
echo 12000 > /proc/sys/vm/dirty_expire_centisecs
echo  6000 > /proc/sys/vm/dirty_writeback_centisecs
echo 131072 > /proc/sys/vm/min_free_kbytes
(sleep 30; cpupower frequency-set -g powersave) &

log Starting X in the background
echo 'startx &'
cd ~rlblaster
PATH=/home/rlblaster/.bin:$PATH su -c "startx -- vt7 -nolisten tcp" rlblaster 2>/dev/null >&2 &
cd - >/dev/null

log Starting ttys
agetty -8 -a rlblaster -o "-p -f rlblaster" --noclear 38400 tty2 linux &
agetty -8 -a rlblaster -o "-p -f rlblaster" --noclear 38400 tty3 linux &
agetty -8 -a rlblaster -o "-p -f rlblaster" --noclear 38400 tty4 linux &
agetty -8 -a rlblaster -o "-p -f rlblaster" --noclear 38400 tty5 linux &
agetty -8 -a rlblaster -o "-p -f rlblaster" --noclear 38400 tty6 linux &
agetty -8 -a rlblaster -o "-p -f rlblaster" --noclear 38400 tty1 linux &
