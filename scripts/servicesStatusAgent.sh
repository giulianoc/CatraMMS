#!/bin/bash

source /opt/catramms/CatraMMS/scripts/servicesStatusLibrary.sh

if [ $# -eq 0 ]
then
	echo "usage $0 moduleType_1 [parameter_1] [moduleType_2 [parameter_2]] [moduleType_3 [parameter_3]]" >> $debugFilename
	echo "	examples:" >> $debugFilename
	echo "	$0 engine" >> $debugFilename
	echo "	$0 api <healthCheckURL>" >> $debugFilename
	echo "	$0 delivery" >> $debugFilename
	echo "	$0 encoder <healthCheckURL>" >> $debugFilename
	echo "	$0 externalEncoder <healthCheckURL>" >> $debugFilename
	echo "	$0 storage" >> $debugFilename
	echo "	$0 integration" >> $debugFilename

	exit
fi


disks_usage
cpu_usage
memory_usage

while [ -n "$1" ]
do
	case "$1" in
		"engine")
			sql_slave_off
			mms_engine_service_running
			;;
		"api")
			nginx_rate_api_limit

			healthCheckURL=$2
			shift
			mms_api_service_running $healthCheckURL
			;;
		"gui")
			nginx_rate_gui_limit
			;;
		"delivery")
			nginx_rate_binary_limit
			nginx_rate_delivery_limit
			;;
		"encoder" | "externalEncoder")
			nginx_rate_encoder_limit

			healthCheckURL=$2
			shift
			mms_encoder_service_running $healthCheckURL
			;;
		"integration")
			;;
        *) echo "$1 is not an option" >> $debugFilename
			;;
	esac
	shift
done

