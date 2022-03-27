#!/bin/bash

if [ $# -ne 1 -a $# -ne 2 ]
then
	echo "Usage $0 start | stop | status | resetdata [nodaemon]"

	exit
fi

command=$1

if [ "$command" != "start" -a "$command" != "stop" -a "$command" != "status" -a "$command" != "resetdata" ]
then
	echo "Usage $0 start | stop | status | resetdata [nodaemon]"

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


PIDFILE=/var/catramms/pids/mmsEngineService.pid

if [ "$2" == "nodaemon" ]
then
	OPTIONS="--nodaemon"
else
	OPTIONS="--pidfile $PIDFILE"
fi

CatraMMS_PATH=/opt/catramms

#used by ImageMagick to look for the configuration files
export MAGICK_CONFIGURE_PATH=$CatraMMS_PATH/ImageMagick/etc/ImageMagick-7
export MAGICK_HOME=$CatraMMS_PATH/ImageMagick

#When Image Magick read a png file we had the error 'error bad parameters to zlib'
#This is probable because of more than one libz available or it is related
#to the version of OS. Solution is the next export. I guess, upgrading the OS,
#we can try to remove the next export
export LD_PRELOAD=libz.so.1

export LD_LIBRARY_PATH=$CatraMMS_PATH/CatraLibraries/lib:$CatraMMS_PATH/CatraMMS/lib:$CatraMMS_PATH/ImageMagick/lib:$CatraMMS_PATH/curlpp/lib:$CatraMMS_PATH/curlpp/lib64:$CatraMMS_PATH/ffmpeg/lib:$CatraMMS_PATH/ffmpeg/lib64:$CatraMMS_PATH/jsoncpp/lib:$CatraMMS_PATH/opencv/lib64:$CatraMMS_PATH/opencv/lib:$CatraMMS_PATH/aws-sdk-cpp/lib

processorShutdownPathName=/tmp/processorShutdown.txt

if [ "$command" == "start" ]
then
	#to be sure there is no shutdown file
	rm -f $processorShutdownPathName

	#to generate a core in case of sigabrt signal
	#1. run below ulimit command
	#2. run mmsEngineService as nodaemon with '&' to run as background
	#3. waiting the core
	#ulimit -c unlimited
	$CatraMMS_PATH/CatraMMS/bin/mmsEngineService $OPTIONS ~/mms/conf/mms.cfg
elif [ "$command" == "resetdata" ]
then
	$CatraMMS_PATH/CatraMMS/bin/mmsEngineService --resetdata ~/mms/conf/mms.cfg
elif [ "$command" == "status" ]
then
	ps -ef | grep "mmsEngineService" | grep -v grep | grep -v status
elif [ "$command" == "stop" ]
then
	sleepIfNeeded

	touch $processorShutdownPathName

	maxSecondsToWait=10
	currentSeconds=0
	isRunning=$(ps -ef | grep "mmsEngineService" | grep -v grep | grep -v stop)
	while [ $currentSeconds -lt $maxSecondsToWait -a "$isRunning" != "" ]
	do
		currentSeconds=$((currentSeconds+1))

		echo "Waiting shutdown ... ($currentSeconds)"
		sleep 1
		isRunning=$(ps -ef | grep "mmsEngineService" | grep -v grep | grep -v stop)
		#echo "isRunning: $isRunning"
	done

	#PIDFILE is not created in case of nodaemon
	if [ "$isRunning" != "" ]
	then
		echo "Shutdown didn't work, process is now killed"

		kill -9 `cat $PIDFILE`
	fi
	
	rm -f $processorShutdownPathName
fi

