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

export JAVA_HOME=/usr/lib/jvm/java-11-openjdk-amd64
export JAVA_OPTS="-Djava.security.egd=file:///dev/urandom -Djava.awt.headless=true"

export CATALINA_BASE=/opt/catramms/tomcat
export CATALINA_HOME=/opt/catramms/tomcat
export CATALINA_PID=/var/catramms/pids/tomcat.pid
export CATALINA_OPTS="-Xms512M -Xmx4096M -server -XX:+UseParallelGC"


if [ "$command" == "start" ]
then
	/opt/catramms/tomcat/bin/startup.sh
elif [ "$command" == "status" ]
then
	ps -ef | grep java | grep -v grep
elif [ "$command" == "stop" ]
then
	/opt/catramms/tomcat/bin/shutdown.sh
	echo "rm -rf /opt/catramms/tomcat/work/Catalina"
	rm -rf /opt/catramms/tomcat/work/Catalina
fi

