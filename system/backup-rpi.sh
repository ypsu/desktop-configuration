#!/bin/bash

destfile=$(date -u "+/mnt/usb/backup-rpi/%Y-%m-%d-%H-%M.img.gz")
echo "Backing up to $destfile."
set -e
set -x

mount -r -o remount /
mount -r -o remount /boot
mount /dev/sda /mnt/usb
gzip --fast </dev/mmcblk0 >$destfile
ls -lh $(dirname $destfile)
umount /mnt/usb
eject /dev/sda
mount -o remount,rw /
mount -o remount,rw /boot
