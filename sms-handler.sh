#!/bin/bash

# command to lower
CMD=${2,,}
INTERFACES="voo ppp0 ppp1"
AUTHORIZE="3249444XXXX"
UNAUTHORIZE="Sorry, phone number '$1' is unauthorized to use this service"

# reading command
case "$CMD" in
	ip)
		for ip in $INTERFACES; do
			ifconfig $ip | egrep 'UP|inet ' | awk '/:/ { printf "%s ", $1 } /inet/ { print $2 }'
		done
		;;
	
	uptime)
		uptime
		;;
	
	uname)
		uname -a
		;;
	
	privmsg)
		/opt/scripts/irc-z03 '#inpres' "[SMS] $1: $3"
		echo "Message sent"
		;;
	
	mpc)
		(mpc -h 192.168.10.210 | head -1) 2>&1
		;;
	
	cocu)
		X=$(($RANDOM % 8))
		echo "Votre (ex-)partenaire vous trompe actuellement avec $X personnes."
		;;
	
	gpio-fan-toggle)
		/opt/websocket/websocket 192.168.10.150 8090 muxberrypi '{"module":"gpio","payload":{"request":"toggle","gpio":8}}' > /dev/null
		echo "toggle done"
		;;
	
	tweet)
		if [ "$1" == "$AUTHORIZE" ]; then
			/opt/scripts/tweet-sender "#twitter_maxuxunix" "post $3"
			echo "(Twitter) tweet sent"
		else
			echo $UNAUTHORIZE
		fi
		;;
	
	reply)
		if [ "$1" == "$AUTHORIZE" ]; then
			/opt/scripts/tweet-sender "#twitter_maxuxunix" "reply $3"
			echo "(Twitter) reply sent"
		else
			echo $UNAUTHORIZE
		fi
		;;

	
	irc)
		if [ "$1" == "$AUTHORIZE" ]; then
			/opt/scripts/privmsg-sender "#inpres" "$3"
			echo "Message sent"
		else
			echo $UNAUTHORIZE
		fi
		;;

	*)
		echo "Unknown command"
esac
