#!/bin/bash

if [ $# -ne 1 -a $# -ne 2 ]
then
	echo "Usage $0 start | stop | status | reload [sudo]"

	exit
fi

command=$1

#sudoToBeUsed could be: sudo or nothing/empty string
sudoToBeUsed=$2


if [ "$command" != "start" -a "$command" != "stop" -a "$command" != "status" -a "$command" != "reload" ]
then
	echo "Usage $0 start | stop  | status | reload[nodaemon]"

	exit
fi

export CatraMMS_PATH=/opt/catramms

if [ "$command" == "start" ]
then
	#LD_LIBRARY_PATH with ffmpeg was set because I guess it could be used by nginx-vod-module
	if [ "$sudoToBeUsed" == "sudo" ]
	then
		$sudoToBeUsed LD_LIBRARY_PATH=$CatraMMS_PATH/ffmpeg/lib:$CatraMMS_PATH/ffmpeg/lib64 $CatraMMS_PATH/nginx/sbin/nginx -p $CatraMMS_PATH/nginx
	else
		export LD_LIBRARY_PATH=$CatraMMS_PATH/ffmpeg/lib:$CatraMMS_PATH/ffmpeg/lib64
		$CatraMMS_PATH/nginx/sbin/nginx -p $CatraMMS_PATH/nginx
	fi
elif [ "$command" == "status" ]
then
	ps -ef | grep nginx | grep -v grep | grep -v status
elif [ "$command" == "stop" ]
then
	if [ "$sudoToBeUsed" == "sudo" ]
	then
		sudo LD_LIBRARY_PATH=$CatraMMS_PATH/ffmpeg/lib:$CatraMMS_PATH/ffmpeg/lib64 $CatraMMS_PATH/nginx/sbin/nginx -s stop
	else
		export LD_LIBRARY_PATH=$CatraMMS_PATH/ffmpeg/lib:$CatraMMS_PATH/ffmpeg/lib64
		timeout 15 $CatraMMS_PATH/nginx/sbin/nginx -s stop
		if [ $? -eq 124 ]
		then
			#timeout expired, let's try a kill
			pkill nginx
		fi
	fi
	#PIDFILE=$(cat $CatraMMS_PATH/nginx/conf/nginx.conf | grep -Ev '^\s*#' | awk 'BEGIN { RS="[;{}]" } { if ($1 == "pid") print $2 }' | head -n1)
	##echo $PIDFILE
	##sudo start-stop-daemon --stop --quiet  --retry=TERM/30/KILL/5 --pidfile $PID --name nginx
	#sudo kill -QUIT $( cat $PIDFILE )
	#sudo pkill nginx
elif [ "$command" == "reload" ]
then
	if [ "$sudoToBeUsed" == "sudo" ]
	then
		sudo LD_LIBRARY_PATH=$CatraMMS_PATH/ffmpeg/lib:$CatraMMS_PATH/ffmpeg/lib64 $CatraMMS_PATH/nginx/sbin/nginx -s reload
	else
		export LD_LIBRARY_PATH=$CatraMMS_PATH/ffmpeg/lib:$CatraMMS_PATH/ffmpeg/lib64
		$CatraMMS_PATH/nginx/sbin/nginx -s reload
	fi
fi

