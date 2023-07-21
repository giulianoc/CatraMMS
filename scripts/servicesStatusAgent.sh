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

echo "" >> $debugFilename

echo "" >> $debugFilename
disks_usage

echo "" >> $debugFilename
cpu_usage

echo "" >> $debugFilename
memory_usage

#to be executed just once (by api-1 test-api-1 server)
#2023-03-25: commentata perchè abbiamo mms_api_timing_check_service che monitora i tempi di tutte le API
#if [ "$(hostname)" = "api-1" ]
#then
#	echo "" >> $debugFilename
#	mms_call_api_service
#elif [ "$(hostname)" = "test-api-1" ]
#then
#	echo "" >> $debugFilename
#	mms_call_api_service "https://mms-api-test.catramms-cloud.com/catramms/1.0.1/mediaItem?start=0&rows=50" "MTpITlZPb1ZoSHgweW9XTkl4RnUtVGhCQTF2QVBFS1dzeG5lR2d6ZTZlb2RmZThQU35BMnhwd0tCZ0RSNDBDaVZk"
#elif [ "$(hostname)" == "ip-172-31-40-201" ]
#then
#	echo "" >> $debugFilename
#	mms_call_api_service "https://mms-api.cibortv-mms.com/catramms/1.0.1/mediaItem?start=0&rows=50" "MjpQS3FKdER0bm1Ud2lPaWE4RjRVN3V4MWF4QTF6S21lSllUUmhvbk1GRTV1cENmRlo3VS1TaWQ0YThkU2Zrc0Rt"
#else
#	echo "" >> $debugFilename
#	echo "mms_call_api_service no to be called. hostname: $(hostname)" >> $debugFilename
#fi

while [ -n "$1" ]
do
	case "$1" in
		"engine")
			echo "" >> $debugFilename
			mount_error

			echo "" >> $debugFilename
			sql_slave_off

			echo "" >> $debugFilename
			sql_check

			echo "" >> $debugFilename
			mms_engine_service_running
			;;
		"api")
			echo "" >> $debugFilename
			nginx_api_error

			echo "" >> $debugFilename
			healthCheckURL=$2
			shift
			mms_api_service_running $healthCheckURL

			echo "" >> $debugFilename
			mms_api_timing_check_service
			;;
		"gui")
			echo "" >> $debugFilename
			nginx_gui_error

			;;
		"delivery")
			echo "" >> $debugFilename
			mount_error

			echo "" >> $debugFilename
			nginx_binary_error

			echo "" >> $debugFilename
			nginx_delivery_error

			;;
		"encoder" | "externalEncoder")
			if [ "$1" = "encoder" ]
			then
				echo "" >> $debugFilename
				mount_error
			fi

			echo "" >> $debugFilename
			nginx_encoder_error

			echo "" >> $debugFilename
			healthCheckURL=$2
			shift
			mms_encoder_service_running $healthCheckURL

			echo "" >> $debugFilename
			ffmpeg_filter_detect blackdetect

			echo "" >> $debugFilename
			ffmpeg_filter_detect blackframe

			echo "" >> $debugFilename
			ffmpeg_filter_detect freezedetect

			echo "" >> $debugFilename
			ffmpeg_filter_detect silencedetect

			;;
		"integration")
			echo "" >> $debugFilename
			nginx_integration_error

			;;
        *) echo "$1 is not an option" >> $debugFilename
			;;
	esac
	shift
done

