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

MMS_PATH=/opt/mms

export LD_LIBRARY_PATH=$MMS_PATH/MMS/lib:$MMS_PATH/libpqxx/lib:$MMS_PATH/ImageMagick/lib:$MMS_PATH/curlpp/lib:$MMS_PATH/curlpp/lib64:$MMS_PATH/ffmpeg/lib:$MMS_PATH/ffmpeg/lib64:$MMS_PATH/jsoncpp/lib:$MMS_PATH/opencv/lib64:$MMS_PATH/opencv/lib:$MMS_PATH/aws-sdk-cpp/lib
source ~/mms/conf/mms-env.sh
export MMS_CONFIGPATHNAME=/opt/mms/MMS/conf/mms.cfg

PIDFILE=/var/mms/pids/api.pid
#port used by nginx (see conf/*.nginx files)
PORT=8010

sleepWaitingLoadBalancer=10

if [ "$command" == "start" ]
then
	spawn-fcgi -p $PORT -P $PIDFILE $FORK_OPTION $MMS_PATH/MMS/bin/cgi/api.fcgi

	if [ "$MMS_ENV" == "prod" ]; then
		privateIPAddress=$(ifconfig | grep "inet 10.0" | grep -Eo '([0-9]*\.){3}[0-9]*' | head -n 1)
		if [ "$privateIPAddress" == "" ]; then
			echo "It is not possible to add server to the load balancer because the Private IP Address is not found"
		else
			echo "Add server to the load balancer: hcloud load-balancer add-target --ip $privateIPAddress mms-delivery-prod"
			hcloud load-balancer add-target --ip $privateIPAddress mms-delivery-prod
			echo "Add server to the load balancer: hcloud load-balancer add-target --ip $privateIPAddress mms-delivery-path-prod"
			hcloud load-balancer add-target --ip $privateIPAddress mms-delivery-path-prod
			echo "Add server to the load balancer: hcloud load-balancer add-target --ip $privateIPAddress mms-delivery-f-prod"
			hcloud load-balancer add-target --ip $privateIPAddress mms-delivery-f-prod
			#nota che il load balancer si accorgera che il servizio è su quando nginx sarà fatto nuovamente ripartire
			echo "Waiting load balancer command ($sleepWaitingLoadBalancer secs) ..."
			sleep $sleepWaitingLoadBalancer
		fi
	fi
elif [ "$command" == "status" ]
then
	ps -ef | grep "api.fcgi" | grep -v grep | grep -v status
elif [ "$command" == "stop" ]
then
	if [ "$MMS_ENV" == "prod" ]; then
		privateIPAddress=$(ifconfig | grep "inet 10.0" | grep -Eo '([0-9]*\.){3}[0-9]*' | head -n 1)
		if [ "$privateIPAddress" == "" ]; then
			echo "It is not possible to remove server from load balancer because the Private IP Address is not found"
		else
				echo "Remove server from the load balancer: hcloud load-balancer remove-target --ip $privateIPAddress mms-delivery-prod"
				hcloud load-balancer remove-target --ip $privateIPAddress mms-delivery-prod
				echo "Remove server from the load balancer: hcloud load-balancer remove-target --ip $privateIPAddress mms-delivery-path-prod"
				hcloud load-balancer remove-target --ip $privateIPAddress mms-delivery-path-prod
				echo "Remove server from the load balancer: hcloud load-balancer remove-target --ip $privateIPAddress mms-delivery-f-prod"
				hcloud load-balancer remove-target --ip $privateIPAddress mms-delivery-f-prod
				echo "Waiting load balancer command ($sleepWaitingLoadBalancer secs) ..."
				sleep $sleepWaitingLoadBalancer
		fi
	fi

	#PIDFILE is not created in case of nodaemon
	kill -9 `cat $PIDFILE`
fi

