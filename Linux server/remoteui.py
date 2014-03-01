#!/usr/bin/python
# RemoteUI server
# Copyright (C) 2013  Federico "MrModd" Cosentino (http://mrmodd.it/)

import serial
import time
import commands
import sys
from subprocess import Popen, PIPE

def sendEntries(ser):
	ser.write("Shutdown\n")
	time.sleep(0.1)
	ser.write("Reboot\n")
	time.sleep(0.1)
	ser.write("Set user mode\n")
	time.sleep(0.1)
	ser.write("Set normal mode\n")
	time.sleep(0.1)
	ser.write("Unset mode\n")
	time.sleep(0.1)
	ser.write("Restart services\n")
	time.sleep(0.1)
	ser.write("Suspend downloads\n")
	time.sleep(0.1)
	ser.write("Resume downloads\n")
	time.sleep(0.1)

if len(sys.argv) != 2:
	print "Usage: " + sys.argv[0] + " <Device File>"
	exit(0)

print "Service starded"
print "Device file: " + sys.argv[1]
ser = serial.Serial(sys.argv[1])

time.sleep(1)

line = ""
while 1:
	line = ser.readline()
	line = line.replace("\r\n", "")
	print "Message: " + line
	if line == "Ready!":
		print "Required entries from device"
		sendEntries(ser)
	elif line == "Shutdown":
		print "System will now shutdown"
		commands.getoutput("echo 'Received shutdown signal from Arduino,\nsystem will now shut down.' | wall")
		commands.getoutput("shutdown -h now")
	elif line == "Reboot":
		print "System will now reboot"
		commands.getoutput("echo 'Received reboot signal from Arduino,\nsystem will now reboot.' | wall")
		commands.getoutput("shutdown -r now")
	elif line == "Set user mode":
		print "Setting user mode override for next bootup"
		commands.getoutput("service custom-start override user")
	elif line == "Set normal mode":
		print "Setting normal mode override for next bootup"
		commands.getoutput("service custom-start override normal")
	elif line == "Unset mode":
		print "Removing mode overriding for next bootup"
		commands.getoutput("service custom-start deloverriding")
	elif line == "Restart services":
		print "Restarting all services"
		commands.getoutput("service custom-start restart")
	elif line == "Suspend downloads":
		print "Suspending all downloads"
		commands.getoutput("service transmission-daemon stop")
		commands.getoutput("service amule-daemon stop")
	elif line == "Resume downloads":
		print "Resuming downloads"
		commands.getoutput("service amule-daemon start")
		commands.getoutput("service transmission-daemon start")
	elif line == "If you want to quit, just send \\n or some wrong bytes.":
		(stdout, stderr) = Popen(["date","+\"%Y-%m-%d %H:%M:%S %w\""], stdout=PIPE).communicate()
		ser.write(stdout.replace("\"", ""))
		time.sleep(0.1)
	else:
		print "Unrecognized command"

ser.close()
