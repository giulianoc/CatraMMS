#!/bin/bash

source /opt/catramms/CatraMMS/scripts/servicesStatusLibrary.sh

if [ $# -eq 0 ]
then
	echo "usage $0 <moduleType_1 [moduleType_2] [moduleType_3] (engine or api or delivery or encoder or externalEncoder or storage or integration)>" >> $debugFilename

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
			;;
		"api")
			nginx_rate_api_limit
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
			;;
		"integration")
			;;
        *) echo "$1 is not an option" >> $debugFilename
			;;
	esac
	shift
done

