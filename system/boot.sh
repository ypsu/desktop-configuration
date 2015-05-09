#!/bin/bash

function logexec() {
	echo "$@"
	"$@"
}

function log() {
	echo -e "\e[33m""$@""\e[0m"
}

function eper() {
	if test "$(hostname)" = "eper"; then
		"$@"
	fi
}

function paks() {
	if test "$(hostname)" = "paks"; then
		"$@"
	fi
}

log Mounting system dirs
logexec mount -t proc proc /proc -o nosuid,noexec,nodev
logexec mount -t sysfs sys /sys -o nosuid,noexec,nodev
logexec mount -t tmpfs run /run -o mode=0755,nosuid,nodev
logexec mkdir -p /dev/pts /dev/shm /run/lock
logexec mount -t devpts devpts /dev/pts -o mode=0620,gid=5,nosuid,noexec
logexec mount -t tmpfs shm /dev/shm -o mode=1777,nosuid,nodev
logexec mount -t tmpfs tmpfs /tmp -o nosuid,nodev,size=50%
mkdir -p /tmp/a
chown rlblaster:users /tmp/a

if grep -q ARMv6 /proc/cpuinfo; then
	hostname eper
else
	hostname paks
fi

log Waiting for udev
logexec /usr/lib/systemd/systemd-udevd --daemon
logexec udevadm trigger --action=add --type=subsystems
logexec udevadm trigger --action=add --type=devices
logexec udevadm settle
setfont -f /usr/share/kbd/consolefonts/ter-v12n.psf.gz

log Bringing up networking
logexec ifconfig lo up
eper logexec ifconfig eth0 up
eper logexec ifconfig eth0 192.168.1.123 netmask 255.255.255.0 broadcast 192.168.1.255
eper logexec route add default gw 192.168.1.1 eth0
paks ifconfig eno1 up
paks logexec ifconfig eno1 192.168.1.200 netmask 255.255.255.0 broadcast 192.168.1.255
paks logexec route add default gw 192.168.1.1 eno1

log Checking filesystems
eper logexec fsck -T -C /
paks fsck -T -C / && fsck -T -C /data
if test $? -ge 2; then
	agetty -8 -a root --noclear 38400 tty1 linux
fi

log Mounting filesystems
logexec mount -o remount,rw /
logexec mount /boot
paks logexec mount /data

echo Setting up environment
export LC_ALL=en_US.UTF-8
export PATH=/root/.sbin:$PATH
eper logexec modprobe snd_bcm2835
paks log Starting dbus
paks install -m755 -g 81 -o 81 -d /run/{dbus,lock}
paks dbus-daemon --system &
logexec alsactl restore
/root/.sbin/basic_syslogd &

log Miscellaneous settings
modprobe loop
echo Setting tty keymap
loadkeys -d
loadkeys /home/rlblaster/proj/desktop-configuration/config/loadkeys.cfg

echo Setting kernel variables
echo 100 > /proc/sys/vm/dirty_background_ratio
echo 100 > /proc/sys/vm/dirty_ratio
echo 30000 > /proc/sys/vm/dirty_expire_centisecs
echo 18000 > /proc/sys/vm/dirty_writeback_centisecs
eper echo 16384 > /proc/sys/vm/min_free_kbytes
paks echo 262144 > /proc/sys/vm/min_free_kbytes

log Setting time
sntp

if test "$(hostname)" = "paks"; then
	log Starting X in the background
	cd ~rlblaster
	PATH=/home/rlblaster/.bin:$PATH su -c "startx -- vt7 -nolisten tcp" rlblaster 2>/dev/null >&2 &
	cd - >/dev/null
fi

log Starting ttys
paks agetty -8 -a rlblaster -o "-p -f rlblaster" --noclear 38400 tty2 linux &
eper agetty -8 -a rlblaster -o "-p -f rlblaster" --noclear 38400 tty2 linux -l /root/.sbin/start_ui.sh &
agetty -8 -a rlblaster -o "-p -f rlblaster" --noclear 38400 tty3 linux &
agetty -8 -a rlblaster -o "-p -f rlblaster" --noclear 38400 tty4 linux &
agetty -8 -a rlblaster -o "-p -f rlblaster" --noclear 38400 tty5 linux &
agetty -8 -a rlblaster -o "-p -f rlblaster" --noclear 38400 tty6 linux &
agetty -8 -a rlblaster -o "-p -f rlblaster" --noclear 38400 tty1 linux &

eper chvt 2

eper su rlblaster -c "/usr/bin/sshd -f /home/rlblaster/.sshd_config"
