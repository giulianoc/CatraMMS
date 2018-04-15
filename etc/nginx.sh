#!/bin/bash

if [ $# -ne 1 ]
then
	echo "Usage $0 start | stop"

	exit
fi

command=$1

if [ "$command" != "start" -a "$command" != "stop" ]
then
	echo "Usage $0 start | stop [nodaemon]"

	exit
fi

if [ "$command" == "start" ]
then
	sudo /usr/local/nginx/sbin/nginx 
elif [ "$command" == "stop" ]
then
	PID=$(cat /usr/local/nginx/conf/nginx.conf | grep -Ev '^\s*#' | awk 'BEGIN { RS="[;{}]" } { if ($1 == "pid") print $2 }' | head -n1)
	#echo $PID
	sudo start-stop-daemon --stop --quiet  --retry=TERM/30/KILL/5 --pidfile $PID --name nginx
fi


