#!/bin/bash

if [ $# -ne 1 -a $# -ne 2 ]
then
	echo "Usage $0 start | stop | status"

	exit
fi

command=$1

if [ "$command" != "start" -a "$command" != "stop" -a "$command" != "status" ]
then
	echo "Usage $0 start | stop  | status"

	exit
fi

PIDFILE=/var/catramms/pids/cibortv.pid

if [ "$command" == "start" ]
then
	/opt/catramms/cibortv-0.1/bin/cibortv &
	pid=$!
	echo "$pid" > $PIDFILE
elif [ "$command" == "status" ]
then
	ps -ef | grep cibortv | grep -v grep | grep -v status
elif [ "$command" == "stop" ]
then
	kill -9 $( cat $PIDFILE )
fi

