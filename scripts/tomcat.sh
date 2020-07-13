#!/bin/bash

if [ $# -ne 1 ]
then
	echo "Usage $0 start | stop | status"

	exit
fi

command=$1

if [ "$command" != "start" -a "$command" != "stop" -a "$command" != "status" ]
then
	echo "Usage $0 start | stop  | status[nodaemon]"

	exit
fi

if [ "$command" == "start" ]
then
	sudo systemctl start tomcat
elif [ "$command" == "status" ]
then
	sudo systemctl status tomcat
elif [ "$command" == "stop" ]
then
	sudo systemctl stop tomcat
	echo "rm -rf /opt/catramms/tomcat/work/Catalina"
	rm -rf /opt/catramms/tomcat/work/Catalina
fi

