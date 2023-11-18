#!/bin/bash

if [ $# -ne 2 ]
then
	echo "Usage $0 command (start | stop | status) service-name (i.e.: cibortv, catrammswebservices)"

	exit
fi


command=$1
serviceName=$2

if [ "$command" != "start" -a "$command" != "stop" -a "$command" != "status" ]
then
	echo "Usage $0 start | stop  | status"

	exit
fi

PIDFILE=/var/catramms/pids/$serviceName.pid

if [ "$command" == "start" ]
then
	/opt/catramms/$serviceName-0.1/bin/$serviceName &
	pid=$!
	echo "$pid" > $PIDFILE
elif [ "$command" == "status" ]
then
	ps -ef | grep $serviceName | grep -v grep | grep -v status
elif [ "$command" == "stop" ]
then
	kill -9 $( cat $PIDFILE )
fi

