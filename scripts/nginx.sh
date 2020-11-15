#!/bin/bash

if [ $# -ne 1 ]
then
	echo "Usage $0 start | stop | status | reload"

	exit
fi

command=$1

if [ "$command" != "start" -a "$command" != "stop" -a "$command" != "status" -a "$command" != "reload" ]
then
	echo "Usage $0 start | stop  | status | reload[nodaemon]"

	exit
fi

export CatraMMS_PATH=/opt/catramms

if [ "$command" == "start" ]
then
	#LD_LIBRARY_PATH with ffmpeg was set because I guess it could be used by nginx-vod-module
	sudo sh -c 'export CatraMMS_PATH=/opt/catramms && export LD_LIBRARY_PATH=$CatraMMS_PATH/ffmpeg/lib:$CatraMMS_PATH/ffmpeg/lib64 && $CatraMMS_PATH/nginx/sbin/nginx -p $CatraMMS_PATH/nginx'
elif [ "$command" == "status" ]
then
	ps -ef | grep nginx | grep -v grep | grep -v status
elif [ "$command" == "stop" ]
then
	PIDFILE=$(cat $CatraMMS_PATH/nginx/conf/nginx.conf | grep -Ev '^\s*#' | awk 'BEGIN { RS="[;{}]" } { if ($1 == "pid") print $2 }' | head -n1)
	#echo $PIDFILE
	#sudo start-stop-daemon --stop --quiet  --retry=TERM/30/KILL/5 --pidfile $PID --name nginx
	sudo kill -QUIT $( cat $PIDFILE )
	sudo pkill nginx
elif [ "$command" == "reload" ]
then
	#LD_LIBRARY_PATH with ffmpeg was set because I guess it could be used by nginx-vod-module
	sudo sh -c 'export CatraMMS_PATH=/opt/catramms && export LD_LIBRARY_PATH=$CatraMMS_PATH/ffmpeg/lib:$CatraMMS_PATH/ffmpeg/lib64 && $CatraMMS_PATH/nginx/sbin/nginx -p $CatraMMS_PATH/nginx -s reload'
fi

