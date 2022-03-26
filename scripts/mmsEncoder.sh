#!/bin/bash

if [ $# -ne 1 -a $# -ne 2 ]
then
	echo "Usage $0 start | stop | status [nodaemon]"

	exit
fi

command=$1

if [ "$command" != "start" -a "$command" != "stop" -a "$command" != "status" ]
then
	echo "Usage $0 start | stop | status [nodaemon]"

	exit
fi

if [ $# -eq 2 -a "$2" != "nodaemon" ]
then
	echo "Usage $0 start | stop | status [nodaemon]"

	exit
fi

sleepIfNeeded()
{
	currentSeconds=$(date +"%-S")
	if [ $currentSeconds -gt 45 ]
	then
		secondsToSleep=$((60-$currentSeconds+10))

		echo "Current seconds: $currentSeconds, sleeping $secondsToSleep"
		sleep $secondsToSleep
	elif [ $currentSeconds -lt 10 ]
	then
		secondsToSleep=$((10-$currentSeconds))

		echo "Current seconds: $currentSeconds, sleeping $secondsToSleep"
		sleep $secondsToSleep
	fi
}


if [ "$2" == "nodaemon" ]
then
	FORK_OPTION="-n"
else
	FORK_OPTION=""
fi

CatraMMS_PATH=/opt/catramms

export LD_LIBRARY_PATH=$CatraMMS_PATH/CatraLibraries/lib:$CatraMMS_PATH/CatraMMS/lib:$CatraMMS_PATH/ImageMagick/lib:$CatraMMS_PATH/curlpp/lib:$CatraMMS_PATH/curlpp/lib64:$CatraMMS_PATH/ffmpeg/lib:$CatraMMS_PATH/ffmpeg/lib64:$CatraMMS_PATH/jsoncpp/lib:$CatraMMS_PATH/opencv/lib64:$CatraMMS_PATH/opencv/lib:$CatraMMS_PATH/aws-sdk-cpp/lib
export MMS_CONFIGPATHNAME=~/mms/conf/mms.cfg

PIDFILE=/var/catramms/pids/encoder.pid
#port used by nginx (see conf/*.nginx files)
PORT=8020

if [ "$command" == "start" ]
then
	spawn-fcgi -p $PORT -P $PIDFILE $FORK_OPTION $CatraMMS_PATH/CatraMMS/bin/cgi/ffmpegEncoder.fcgi
elif [ "$command" == "status" ]
then
	ps -ef | grep ffmpeg | grep -v grep
	ps -ef | grep "ffmpegEncoder.fcgi" | grep -v grep | grep -v status
elif [ "$command" == "stop" ]
then
	sleepIfNeeded

	#PIDFILE is not created in case of nodaemon
	kill -9 `cat $PIDFILE`
	pkill ffmpeg
	sleep 2
	ps -ef | grep ffmpeg | grep -v grep
fi

