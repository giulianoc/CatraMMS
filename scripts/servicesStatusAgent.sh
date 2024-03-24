#!/bin/bash

source /opt/catramms/CatraMMS/scripts/servicesStatusLibrary.sh

if [ $# -eq 0 ]
then
	echo "usage $0 moduleType_1 [parameter_1] [moduleType_2 [parameter_2]] [moduleType_3 [parameter_3]]" >> $debugFilename
	echo "	examples:" >> $debugFilename
	echo "	$0 engine" >> $debugFilename
	echo "	$0 api <baseAPIURL>" >> $debugFilename        #http://10.0.0.5:8088/catramms/1.0.1
	echo "	$0 webapi <baseWEBAPIURL>" >> $debugFilename  #http://10.0.0.5:8086/catramms/webapi/1.0.0
	echo "	$0 delivery" >> $debugFilename
	echo "	$0 encoder <baseEncoderURL>" >> $debugFilename  #http://10.0.1.5:8088/catramms/v1/encoder
	echo "	$0 externalEncoder <healthCheckURL> >> $debugFilename #http://localhost:8088/catramms/v1/encoder
	echo "	$0 storage" >> $debugFilename
	echo "	$0 integration <healthCheckURL>" >> $debugFilename  #http://localhost:8084/cibortv/rest/api-v1

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
			postgres_replication_check

			echo "" >> $debugFilename
			sql_check

			echo "" >> $debugFilename
			postgres_check

			echo "" >> $debugFilename
			serviceName=engine
			processName=mmsEngineService
			mms_service_running_by_processName $serviceName $processName

			echo "" >> $debugFilename
			serviceName=engine
			mms_sql_timing_check_service $serviceName
			;;
		"api")
			echo "" >> $debugFilename
			nginx_error api

			echo "" >> $debugFilename
			serviceName=api
			healthCheckURL=$2/status
			mms_service_running_by_healthCheckURL $serviceName "$healthCheckURL"

			echo "" >> $debugFilename
			mms_api_timing_check_service

			echo "" >> $debugFilename
			serviceName=api
			mms_sql_timing_check_service $serviceName

			shift

			;;
		"webapi")
			echo "" >> $debugFilename
			nginx_error webapi

			echo "" >> $debugFilename
			serviceName=webapi
			healthCheckURL=$2/status
			mms_service_running_by_healthCheckURL $serviceName "$healthCheckURL"

			#echo "" >> $debugFilename
			mms_webservices_timing_check_service catraMMSWEBServices catraMMSWEBServices

			shift

			;;
		"gui")
			echo "" >> $debugFilename
			nginx_error gui

			;;
		"delivery")
			echo "" >> $debugFilename
			mount_error

			echo "" >> $debugFilename
			nginx_error binary

			echo "" >> $debugFilename
			nginx_error delivery

			echo "" >> $debugFilename
			nginx_error delivery-f

			echo "" >> $debugFilename
			nginx_error delivery-path

			;;
		"encoder" | "externalEncoder")
			if [ "$1" = "encoder" ]
			then
				echo "" >> $debugFilename
				mount_error
			fi

			echo "" >> $debugFilename
			nginx_error encoder

			echo "" >> $debugFilename

			serviceName=encoder
      baseEncoderURL=$2
			healthCheckURL=$baseEncoderURL/status
      encoderAPIUser=1
      encoderAPIPassword=SU1.8ZO1O2zVeBMNv9lzZ0whABXSAdjWrR~rpcnI5eaHu3Iy6W94kQvSd4cJm.el3j

			mms_service_running_by_healthCheckURL $serviceName "$healthCheckURL"

			echo "" >> $debugFilename
			ffmpeg_filter_detect blackdetect "$baseEncoderURL" "$encoderAPIUser" "$encoderAPIPassword"

			echo "" >> $debugFilename
			ffmpeg_filter_detect blackframe "$baseEncoderURL" "$encoderAPIUser" "$encoderAPIPassword"

			echo "" >> $debugFilename
			ffmpeg_filter_detect freezedetect "$baseEncoderURL" "$encoderAPIUser" "$encoderAPIPassword"

			echo "" >> $debugFilename
			ffmpeg_filter_detect silencedetect "$baseEncoderURL" "$encoderAPIUser" "$encoderAPIPassword"

			shift

			;;
		"integration")
			echo "" >> $debugFilename
			nginx_error cibortv

			echo "" >> $debugFilename
			nginx_error cibortv-epg

			echo "" >> $debugFilename
			nginx_error cibortv-apk

			echo "" >> $debugFilename
			serviceName=cibortv
			healthCheckURL=$2/status
			mms_service_running_by_healthCheckURL $serviceName "$healthCheckURL"

			echo "" >> $debugFilename
			mms_webservices_timing_check_service cibortv cibortv
			mms_webservices_timing_check_service cibortv license

			shift

			;;
        *) echo "$1 is not an option" >> $debugFilename
			;;
	esac
	shift
done

