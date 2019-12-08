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


if [ "$2" == "nodaemon" ]
then
	FORK_OPTION="-n"
else
	FORK_OPTION=""
fi

CatraMMS_PATH=/opt/catramms

export LD_LIBRARY_PATH=$CatraMMS_PATH/CatraLibraries/lib:$CatraMMS_PATH/CatraMMS/lib:$CatraMMS_PATH/ImageMagick-7.0.8-49/lib:$CatraMMS_PATH/curlpp/lib:$CatraMMS_PATH/ffmpeg-4.1.3/lib:$CatraMMS_PATH/ffmpeg-4.1.3/lib64:$CatraMMS_PATH/jsoncpp/lib:$CatraMMS_PATH/opencv/lib64
export MMS_CONFIGPATHNAME=~/mms/conf/mms.cfg

PIDFILE=/var/catramms/pids/encoder.pid
PORT=8020

if [ "$command" == "start" ]
then
	spawn-fcgi -p $PORT -P $PIDFILE $FORK_OPTION $CatraMMS_PATH/CatraMMS/bin/cgi/ffmpegEncoder.fcgi
elif [ "$command" == "status" ]
then
	ps -ef | grep "ffmpegEncoder.fcgi" | grep -v grep | grep -v status
	ps -ef | grep ffmpeg | grep -v grep
elif [ "$command" == "stop" ]
then
	#PIDFILE is not created in case of nodaemon
	kill -9 `cat $PIDFILE`
	pkill ffmpeg
	sleep 2
	ps -ef | grep ffmpeg | grep -v grep
fi

