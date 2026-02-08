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

export JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64
export JAVA_OPTS="-Djava.security.egd=file:///dev/urandom -Djava.awt.headless=true"

export CATALINA_BASE=/opt/mms/tomee
export CATALINA_HOME=/opt/mms/tomee
export CATALINA_PID=/var/mms/pids/tomee.pid
export CATALINA_OPTS="-Xms512M -Xmx4096M -server -XX:+UseParallelGC"


if [ "$command" == "start" ]
then
	/opt/mms/tomee/bin/startup.sh
elif [ "$command" == "status" ]
then
	ps -ef | grep java | grep -v grep
elif [ "$command" == "stop" ]
then
	/opt/mms/tomee/bin/shutdown.sh
	echo "rm -rf /opt/mms/tomee/work/Catalina"
	rm -rf /opt/mms/tomee/work/Catalina
fi

