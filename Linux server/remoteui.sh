#!/bin/bash
# RemoteUI server
# Copyright (C) 2013  Federico "MrModd" Cosentino (http://mrmodd.it/)

# Device to monitor (this must be the Arduino)
DEVICE="/dev/ttyACM0"
# Python script to launch when Arduino is detected
SCRIPT="/opt/remoteui/remoteui.py"

# Check root permissions
if [ "$(id -u)" != "0" ] ; then
	echo "You must be root to execute this script" 1>&2
	exit 1
fi

# Check if device is already connected (eg. during bootup)
if [ -c "$DEVICE" ]
then
	"$SCRIPT" "$DEVICE" 2> /dev/null
	echo "Service ended"
fi

while true
do
	# You must install Debian/Ubuntu package inotify-tools
	DEV=$(inotifywait -e create /dev -q --format %w%f)
	if [[ $DEV =~ /dev/ttyACM[0-9]* ]] # String regex match
	then
		"$SCRIPT" "$DEV" 2> /dev/null
		echo "Service ended"
	fi
done
