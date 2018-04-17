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


PIDFILE=/usr/local/CatraMMS/pids/mmsEngineService.pid

if [ "$2" == "nodaemon" ]
then
	OPTIONS="--nodaemon"
else
	OPTIONS="--pidfile $PIDFILE"
fi

CatraMMS_PATH=/home/mms/catramms

export LD_LIBRARY_PATH=$CatraMMS_PATH/CatraLibraries/lib:$CatraMMS_PATH/CatraMMS/lib:$CatraMMS_PATH/ImageMagick-7.0.7-22/lib:$CatraMMS_PATH/curlpp//lib:$CatraMMS_PATH/ffmpeg-3.4.2/lib:$CatraMMS_PATH/jsoncpp/lib:$CatraMMS_PATH/mysql-connector-c++-1.1.9-linux-ubuntu16.10-x86-64bit/lib

if [ "$command" == "start" ]
then
	$CatraMMS_PATH/CatraMMS/bin/mmsEngineService $OPTIONS $CatraMMS_PATH/CatraMMS/etc/mms.cfg
elif [ "$command" == "status" ]
then
	ps -ef | grep "mmsEngineService"
elif [ "$command" == "stop" ]
then
	#PIDFILE is not created in case of nodaemon
	kill -9 `cat $PIDFILE`
fi

