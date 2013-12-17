#!/bin/bash

# command to lower
CMD=${1,,}

# reading command
case "$CMD" in
	ip)
		ip a | egrep 'UP|inet ' | awk '/:/ { printf "%s ", $2 } /inet/ { print $2 }' 2>&1
		;;
	
	uptime)
		uptime
		;;
	
	uname)
		uname -a
		;;
	
	privmsg)
		/opt/scripts/irc-sender '#inpres' "[SMS] $2"
		echo "Message sent"
		;;
	
	mpc)
		mpc -h 192.168.10.210 | head -1 2>&1
		;;
	
	cocu)
		X=$(($RANDOM % 8))
		echo "Votre (ex-)partenaire vous trompe actuellement avec $X personnes."
		;;
	
	*)
		echo "Unknown command"
esac
