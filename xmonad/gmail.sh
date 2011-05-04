#!/bin/bash

while true; do
	if python2.7 ~/.xmonad/check_gmail.py; then
		touch /dev/shm/B
	else
		rm -f /dev/shm/B
	fi
	sleep 123.45
done
