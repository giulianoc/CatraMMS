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


PIDFILE=/var/CatraMMSData/pids/mmsEngineService.pid

if [ "$2" == "nodaemon" ]
then
	OPTIONS="--nodaemon"
else
	OPTIONS="--pidfile $PIDFILE"
fi

CatraMMS_PATH=/home/mms/catramms

#used by ImageMagick to look for the configuration files
export MAGICK_CONFIGURE_PATH=$CatraMMS_PATH/ImageMagick-7.0.8-10/etc/ImageMagick-7

#When Image Magick read a png file we had the error 'error bad parameters to zlib'
#This is probable because of more than one libz available or it is related
#to the version of OS. Solution is the next export. I guess, upgrading the OS,
#we can try to remove the next export
export LD_PRELOAD=libz.so.1

export LD_LIBRARY_PATH=$CatraMMS_PATH/CatraLibraries/lib:$CatraMMS_PATH/CatraMMS/lib:$CatraMMS_PATH/ImageMagick-7.0.8-10/lib:$CatraMMS_PATH/curlpp//lib:$CatraMMS_PATH/ffmpeg-4.0.2/lib:$CatraMMS_PATH/jsoncpp/lib:$CatraMMS_PATH/mysql-connector-c++-1.1.9-linux-ubuntu16.10-x86-64bit/lib

if [ "$command" == "start" ]
then
	#to generate a core in case of sigabrt signal
	#1. run below ulimit command
	#2. run mmsEngineService as nodaemon with '&' to run as background
	#3. waiting the core
	#ulimit -c unlimited
	$CatraMMS_PATH/CatraMMS/bin/mmsEngineService $OPTIONS $CatraMMS_PATH/CatraMMS/etc/mms.cfg
elif [ "$command" == "status" ]
then
	ps -ef | grep "mmsEngineService" | grep -v grep | grep -v status
elif [ "$command" == "stop" ]
then
	#PIDFILE is not created in case of nodaemon
	kill -9 `cat $PIDFILE`
fi

