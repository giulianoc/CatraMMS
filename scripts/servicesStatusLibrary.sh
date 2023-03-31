#!/bin/bash

debugFilename=/tmp/servicesStatus.log
debug=1

if [ ! -f "$debugFilename" ]; then
	echo "" > $debugFilename
else
	filesize=$(stat -c %s $debugFilename)
	if [ $filesize -gt 10000000 ]
	then
		echo "" > $debugFilename
	fi
fi


getAlarmDescription()
{
	alarmType=$1

	case $alarmType in
		"alarm_sql_slave_off")
			echo "SQL SLAVE is not working"
			;;
		"alarm_disks_usage")
			echo "File system full"
			;;
		"alarm_cpu_usage")
			echo "CPU Usage too high"
			;;
		"alarm_memory_usage")
			echo "Memory Usage too high"
			;;
		"alarm_nginx_gui_error")
			echo "Nginx GUI Error"
			;;
		"alarm_nginx_api_error")
			echo "Nginx API Error"
			;;
		"alarm_nginx_delivery_error")
			echo "Nginx Delivery Error"
			;;
		"alarm_nginx_binary_error")
			echo "Nginx Binary Error"
			;;
		"alarm_nginx_encoder_error")
			echo "Nginx Encoder Error"
			;;
		"alarm_nginx_integration_error")
			echo "Nginx Integration Error"
			;;
		"alarm_mms_engine_service_running")
			echo "mms engine service is not running"
			;;
		"alarm_mms_api_service_running")
			echo "mms api service is not running"
			;;
		"alarm_mms_encoder_service_running")
			echo "mms encoder service is not running"
			;;
		"alarm_blackdetect")
			echo "got blackdetect"
			;;
		"alarm_blackframe")
			echo "got blackframe"
			;;
		"alarm_freezedetect")
			echo "got freezedetect"
			;;
		"alarm_silencedetect")
			echo "got silencedetect"
			;;
		"alarm_mms_call_api_service")
			echo "mms call api service not working or too slow"
			;;
		"alarm_mms_api_timing_check_service")
			echo "mms api too slow"
			;;
		*)
			echo "Unknown alarmType: $alarmType"
			echo "$(date +'%Y/%m/%d %H:%M:%S'): Unknown alarmType: $alarmType" >> $debugFilename
			;;
	esac
}

notify()
{
	serverName=$1
	alarmType=$2
	notifyFileName=$3
	alarmNotificationPeriodInSeconds=$4
	alarmDetails=$5

	alarmNotificationPathFileName="/tmp/$notifyFileName"

	#controllo se è troppo presto per rimandare l'allarme
	if [ -f "$alarmNotificationPathFileName" ]; then
		lastNotificationTime=$(date -r "$alarmNotificationPathFileName" +%s)
		now=$(date +%s)
		elapsed=$((now-lastNotificationTime))
		if [ $elapsed -lt $alarmNotificationPeriodInSeconds ]; then
			echo "$(date +'%Y/%m/%d %H:%M:%S'): $alarmType not sent because too early, elapsed: $elapsed secs" >> $debugFilename
			return 1
		fi
	fi

	echo "$(date +'%Y/%m/%d %H:%M:%S'): Sending $alarmType" >> $debugFilename

	touch $alarmNotificationPathFileName

	alarmDescription=$(getAlarmDescription $alarmType)
	message="$(date +'%Y/%m/%d %H:%M:%S') - ${serverName} - ${alarmDescription} - ${alarmDetails}"

	curl -X GET	\
		--data-urlencode "chat_id=${TELEGRAM_GROUPALARMS_ID}" \
		--data-urlencode "text=${message}" \
		"https://api.telegram.org/bot${TELEGRAM_GROUPALARMS_BOT_TOKEN}/sendMessage" > /dev/null

	return 0
}

sql_slave_off()
{
	#replication_connection_status.service_state (Slave_IO_Running) ON indica che è stato eseguito il comando SQL: start slave.
	#Nello scenario in cui abbiamo un problema, replication_connection_status.service_state rimane ON mentre
	#replication_applier_status_by_coordinator.service_state (Slave_SQL_Running) è OFF
	isMysqlSlave=$(echo "select service_state from performance_schema.replication_connection_status" | mysql -N -u ${MYSQL_USER} -p${MYSQL_PASSWORD} -h localhost ${MYSQL_DBNAME})
	if [ "$isMysqlSlave" == "ON" ]; then

		#solo se è uno slave verifico il suo stato
		mysqlSlaveStatus=$(echo "select service_state from performance_schema.replication_applier_status_by_coordinator" | mysql -N -u ${MYSQL_USER} -p${MYSQL_PASSWORD} -h localhost ${MYSQL_DBNAME})
		if [ "$mysqlSlaveStatus" == "ON" ]; then
			echo "$(date +'%Y/%m/%d %H:%M:%S'): sql_slave_off, slave is working fine" >> $debugFilename

			alarmNotificationPathFileName="/tmp/alarm_sql_slave_off"
			if [ -f "$alarmNotificationPathFileName" ]; then
				rm -f $alarmNotificationPathFileName
			fi

			return 0
		else
			alarmNotificationPeriod=$((60 * 15))		#15 minuti
			notify "$(hostname)" "alarm_sql_slave_off" "alarm_sql_slave_off" $alarmNotificationPeriod ""
			return 1
		fi
	else
		echo "$(date +'%Y/%m/%d %H:%M:%S'): sql_slave_off, it is not a slave" >> $debugFilename

		return 0
	fi
}

disks_usage()
{
	alarmsDiskUsage=$(df -Ph | grep -v "/snap/" | awk 'BEGIN { alarms=""; maxDiskUsage=70; } { if (NR == 1) next; usagePercentage=substr($5, 0, length($5)-1)+0; if (usagePercentage > maxDiskUsage) { alarms=alarms$6" -> "usagePercentage"%; "; } } END {printf("%s", alarms) } ')
	if [ "$alarmsDiskUsage" == "" ]; then
		echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_disks_usage, disks usage is fine" >> $debugFilename

		alarmNotificationPathFileName="/tmp/alarm_disks_usage"
		if [ -f "$alarmNotificationPathFileName" ]; then
			rm -f $alarmNotificationPathFileName
		fi

		return 0
	else
		alarmNotificationPeriod=$((60 * 15))		#15 minuti
		notify "$(hostname)" "alarm_disks_usage" "alarm_disks_usage" $alarmNotificationPeriod "$alarmsDiskUsage"
		return 1
	fi
}

cpu_usage()
{
	maxCpuUsage=60.0

	cpuUsage=$(cat /proc/stat | grep "cpu " | awk '{ printf("%.2f", 100-(($5*100)/($2+$3+$4+$5+$6+$7+$8+$9+$10))); }')
	result=$(echo "${cpuUsage}<${maxCpuUsage}" | bc)                                                                   
	if [ $result = 1 ]; then
		echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_cpu_usage, cpu usage is fine: $cpuUsage" >> $debugFilename

		alarmNotificationPathFileName="/tmp/alarm_cpu_usage"
		if [ -f "$alarmNotificationPathFileName" ]; then
			rm -f $alarmNotificationPathFileName
		fi

		return 0
	else
		alarmNotificationPeriod=$((60 * 5))		#5 minuti
		notify "$(hostname)" "alarm_cpu_usage" "alarm_cpu_usage" $alarmNotificationPeriod "${cpuUsage}%"
		return 1
	fi
}

memory_usage()
{
	maxMemoryUsage=60.0

	memoryUsage=$(free -m | awk '{if (NR==2)printf("%.2f", $3*100/$2)}')
	result=$(echo "${memoryUsage}<${maxMemoryUsage}" | bc)                                                                   
	if [ $result = 1 ]; then
		echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_memory_usage, memory usage is fine: $memoryUsage" >> $debugFilename

		alarmNotificationPathFileName="/tmp/alarm_memory_usage"
		if [ -f "$alarmNotificationPathFileName" ]; then
			rm -f $alarmNotificationPathFileName
		fi

		return 0
	else
		alarmNotificationPeriod=$((60 * 5))		#5 minuti
		notify "$(hostname)" "alarm_memory_usage" "alarm_memory_usage" $alarmNotificationPeriod "${memoryUsage}%"
		return 1
	fi
}

nginx_api_error()
{
	#aggiungo la data/ora come filtro altrimenti ritornerebbe sempre l'errore per tutto il giorno
	dateFilter=$(date +'%Y/%m/%d %H:')
	nginxErrorsCount=$(grep "${dateFilter}" /var/catramms/logs/nginx/mms-api.error.log | grep -v "No such file or directory" | grep -v "is forbidden" | grep -v "Stale file handle" | wc -l)

	if [ $nginxErrorsCount -eq 0 ]; then
		echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_nginx_api_error, nginx API error is fine: $nginxErrorsCount" >> $debugFilename

		alarmNotificationPathFileName="/tmp/alarm_nginx_api_error"
		if [ -f "$alarmNotificationPathFileName" ]; then
			rm -f $alarmNotificationPathFileName
		fi

		return 0
	else
		alarmNotificationPeriod=$((60 * 60))		#60 minuti
		notify "$(hostname)" "alarm_nginx_api_error" "alarm_nginx_api_error" $alarmNotificationPeriod "got ${nginxErrorsCount} times"
		return 1
	fi
}

nginx_binary_error()
{
	#aggiungo la data/ora come filtro altrimenti ritornerebbe sempre l'errore per tutto il giorno
	dateFilter=$(date +'%Y/%m/%d %H:')
	nginxErrorsCount=$(grep "${dateFilter}" /var/catramms/logs/nginx/mms-binary.error.log | grep -v "No such file or directory" | grep -v "is forbidden" | grep -v "Stale file handle" | wc -l)

	if [ $nginxErrorsCount -eq 0 ]; then
		echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_nginx_binary_error, nginx Binary error is fine: $nginxErrorsCount" >> $debugFilename

		alarmNotificationPathFileName="/tmp/alarm_nginx_binary_error"
		if [ -f "$alarmNotificationPathFileName" ]; then
			rm -f $alarmNotificationPathFileName
		fi

		return 0
	else
		alarmNotificationPeriod=$((60 * 60))		#60 minuti
		notify "$(hostname)" "alarm_nginx_binary_error" "alarm_nginx_binary_error" $alarmNotificationPeriod "got ${nginxErrorsCount} times"
		return 1
	fi
}

nginx_delivery_error()
{
	#aggiungo la data/ora come filtro altrimenti ritornerebbe sempre l'errore per tutto il giorno
	dateFilter=$(date +'%Y/%m/%d %H:')
	nginxErrorsCount=$(grep "${dateFilter}" /var/catramms/logs/nginx/mms-delivery*.error.log | grep -v "No such file or directory" | grep -v "is forbidden" | grep -v "Stale file handle" | wc -l)

	if [ $nginxErrorsCount -eq 0 ]; then
		echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_nginx_delivery_error, nginx Delivery error is fine: $nginxErrorsCount" >> $debugFilename

		alarmNotificationPathFileName="/tmp/alarm_nginx_delivery_error"
		if [ -f "$alarmNotificationPathFileName" ]; then
			rm -f $alarmNotificationPathFileName
		fi

		return 0
	else
		alarmNotificationPeriod=$((60 * 60))		#60 minuti
		notify "$(hostname)" "alarm_nginx_delivery_error" "alarm_nginx_delivery_error" $alarmNotificationPeriod "got ${nginxErrorsCount} times"
		return 1
	fi
}

nginx_encoder_error()
{
	#aggiungo la data/ora come filtro altrimenti ritornerebbe sempre l'errore per tutto il giorno
	dateFilter=$(date +'%Y/%m/%d %H:')
	nginxErrorsCount=$(grep "${dateFilter}" /var/catramms/logs/nginx/mms-encoder.error.log | grep -v "No such file or directory" | grep -v "is forbidden" | grep -v "Stale file handle" | wc -l)

	if [ $nginxErrorsCount -eq 0 ]; then
		echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_nginx_encoder_error, nginx Encoder error is fine: $nginxErrorsCount" >> $debugFilename

		alarmNotificationPathFileName="/tmp/alarm_nginx_encoder_error"
		if [ -f "$alarmNotificationPathFileName" ]; then
			rm -f $alarmNotificationPathFileName
		fi

		return 0
	else
		alarmNotificationPeriod=$((60 * 60))		#60 minuti
		notify "$(hostname)" "alarm_nginx_encoder_error" "alarm_nginx_encoder_error" $alarmNotificationPeriod "got ${nginxErrorsCount} times"
		return 1
	fi
}

nginx_gui_error()
{
	#aggiungo la data/ora come filtro altrimenti ritornerebbe sempre l'errore per tutto il giorno
	dateFilter=$(date +'%Y/%m/%d %H:')
	nginxErrorsCount=$(grep "${dateFilter}" /var/catramms/logs/nginx/mms-gui.error.log | grep -v "No such file or directory" | grep -v "is forbidden" | grep -v "Stale file handle" | wc -l)

	if [ $nginxErrorsCount -eq 0 ]; then
		echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_nginx_gui_error, nginx GUI error is fine: $nginxErrorsCount" >> $debugFilename

		alarmNotificationPathFileName="/tmp/alarm_nginx_gui_error"
		if [ -f "$alarmNotificationPathFileName" ]; then
			rm -f $alarmNotificationPathFileName
		fi

		return 0
	else
		alarmNotificationPeriod=$((60 * 60))		#60 minuti
		notify "$(hostname)" "alarm_nginx_gui_error" "alarm_nginx_gui_error" $alarmNotificationPeriod "got ${nginxErrorsCount} times"
		return 1
	fi
}

nginx_integration_error()
{
	#aggiungo la data/ora come filtro altrimenti ritornerebbe sempre l'errore per tutto il giorno
	dateFilter=$(date +'%Y/%m/%d %H:')
	nginxErrorsCount=$(grep "${dateFilter}" /var/catramms/logs/nginx/*.error.log | grep -v "No such file or directory" | grep -v "is forbidden" | grep -v "Stale file handle" | wc -l)

	if [ $nginxErrorsCount -eq 0 ]; then
		echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_nginx_integration_error, nginx Integration error is fine: $nginxErrorsCount" >> $debugFilename

		alarmNotificationPathFileName="/tmp/alarm_nginx_integration_error"
		if [ -f "$alarmNotificationPathFileName" ]; then
			rm -f $alarmNotificationPathFileName
		fi

		return 0
	else
		alarmNotificationPeriod=$((60 * 60))		#60 minuti
		notify "$(hostname)" "alarm_nginx_integration_error" "alarm_nginx_integration_error" $alarmNotificationPeriod "got ${nginxErrorsCount} times"
		return 1
	fi
}

mms_engine_service_running()
{
	pgrep -f mmsEngineService > /dev/null
	serviceNotRunning=$?

	if [ $serviceNotRunning -eq 1 ]
	then
		alarmNotificationPeriod=$((60 * 1))		#1 minuti
		notify "$(hostname)" "alarm_mms_engine_service_running" "alarm_mms_engine_service_running" $alarmNotificationPeriod ""

		#fix management
		echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_mms_engine_service_running, service is restarted" >> $debugFilename

		~/mmsStopALL.sh
		sleep 1
		~/mmsStartALL.sh

		return 1
	else
		echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_mms_engine_service_running, mmsEngineService is running" >> $debugFilename

		alarmNotificationPathFileName="/tmp/alarm_mms_engine_service_running"
		if [ -f "$alarmNotificationPathFileName" ]; then
			rm -f $alarmNotificationPathFileName
		fi

		return 0
	fi
}

mms_api_service_running()
{
	healthCheckURL=$1

	outputHealthCheckURL=/tmp/mms_api_service_running.healthCheckURL.response
	httpStatus=$(curl -k --output $outputHealthCheckURL -w "%{http_code}" --max-time 20 "$healthCheckURL")
	if [ $httpStatus -ne 200 ]
	then
		echo "$(date +'%Y/%m/%d %H:%M:%S'): mms_api_service_running failed, httpStatus: $httpStatus, outputHealthCheckURL: $(cat $outputHealthCheckURL)" >> $debugFilename

		failuresNumberFileName=/tmp/alarm_mms_api_service_running.failuresNumber.txt
		if [ -s $failuresNumberFileName ]
		then
			#exist and is not empty
			failuresNumber=$(cat $failuresNumberFileName)
		else
			failuresNumber=0
		fi
		maxFailuresNumber=3
		alarmNotificationPeriod=$((60 * 1))		#1 minuti
		notify "$(hostname)" "alarm_mms_api_service_running" "alarm_mms_api_service_running" $alarmNotificationPeriod "healthCheckURL: $healthCheckURL"

		#fix management
		if [ $failuresNumber -ge $maxFailuresNumber ]
		then
			echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_mms_api_service_running, service is restarted" >> $debugFilename

			~/mmsStopALL.sh
			sleep 1
			~/mmsStartALL.sh

			rm -f $failuresNumberFileName
		else
			failuresNumber=$((failuresNumber+1))

			echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_mms_api_service_running, one more failure: $failuresNumber, healthCheckURL: $healthCheckURL" >> $debugFilename

			echo "$failuresNumber" > $failuresNumberFileName
		fi

		return 1
	else
		echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_mms_api_service_running, mms api is running" >> $debugFilename

		alarmNotificationPathFileName="/tmp/alarm_mms_api_service_running"
		if [ -f "$alarmNotificationPathFileName" ]; then
			rm -f $alarmNotificationPathFileName
		fi

		#fix management
		failuresNumberFileName=/tmp/alarm_mms_api_service_running.failuresNumber.txt
		if [ -f "$failuresNumberFileName" ]; then
			rm -f $failuresNumberFileName
		fi

		return 0
	fi
}

mms_encoder_service_running()
{
	healthCheckURL=$1

	outputHealthCheckURL=/tmp/mms_encoder_service_running.healthCheckURL.response
	httpStatus=$(curl -k --output $outputHealthCheckURL -w "%{http_code}" --max-time 20 "$healthCheckURL")
	if [ $httpStatus -ne 200 ]
	then
		echo "$(date +'%Y/%m/%d %H:%M:%S'): mms_encoder_service_running failed, httpStatus: $httpStatus, outputHealthCheckURL: $(cat $outputHealthCheckURL)" >> $debugFilename

		failuresNumberFileName=/tmp/alarm_mms_encoder_service_running.failuresNumber.txt
		if [ -s $failuresNumberFileName ]
		then
			#exist and is not empty
			failuresNumber=$(cat $failuresNumberFileName)
		else
			failuresNumber=0
		fi
		maxFailuresNumber=1000
		alarmNotificationPeriod=$((60 * 1))		#1 minuti
		notify "$(hostname)" "alarm_mms_encoder_service_running" "alarm_mms_encoder_service_running" $alarmNotificationPeriod "healthCheckURL: $healthCheckURL, failuresNumber: $failuresNumber/$maxFailuresNumber"

		#fix management
		if [ $failuresNumber -ge $maxFailuresNumber ]
		then
			echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_mms_encoder_service_running, service is restarted" >> $debugFilename

			~/mmsStopALL.sh
			sleep 1
			~/mmsStartALL.sh

			rm -f $failuresNumberFileName
		else
			failuresNumber=$((failuresNumber+1))

			echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_mms_encoder_service_running, one more failure: $failuresNumber, serviceStatus: $serviceStatus, healthCheckURL: $healthCheckURL" >> $debugFilename

			echo "$failuresNumber" > $failuresNumberFileName
		fi

		return 1
	else
		echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_mms_encoder_service_running, mms encoder is running" >> $debugFilename

		alarmNotificationPathFileName="/tmp/alarm_mms_encoder_service_running"
		if [ -f "$alarmNotificationPathFileName" ]; then
			rm -f $alarmNotificationPathFileName
		fi

		#fix management
		failuresNumberFileName=/tmp/alarm_mms_encoder_service_running.failuresNumber.txt
		if [ -f "$failuresNumberFileName" ]; then
			rm -f $failuresNumberFileName
		fi

		return 0
	fi
}

ffmpeg_filter_detect()
{
	filterName=$1

	for logFile in /var/catramms/storage/MMSTranscoderWorkingAreaRepository/ffmpeg/*.log
	do
		fileName=$(basename $logFile)
		filterCount=$(grep "$filterName" $logFile | wc -l)

		alarmNotificationPathFileName="/tmp/alarm_${filterName}_${fileName}"
		infoPathFileName="${alarmNotificationPathFileName}_info"
		if [ $filterCount -eq 0 ]; then
			echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_$filterName, filterCount: $filterCount, logFile: $logFile" >> $debugFilename

			if [ -f "$alarmNotificationPathFileName" ]; then
				rm -f $alarmNotificationPathFileName
			fi
			if [ -f "$infoPathFileName" ]; then
				rm -f $infoPathFileName
			fi
		else
			#controllo il contatore precedente per verificare che è aumentato
			counterIncreased=0
			if [ -f "$infoPathFileName" ]; then
				previousFilterCount=$(cat $infoPathFileName)
				if [ $filterCount -ne $previousFilterCount ]; then
					counterIncreased=1
				else
					echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_$filterName, filterCount: $filterCount, previousFilterCount: $previousFilterCount, logFile: $logFile" >> $debugFilename
				fi
			else
				counterIncreased=1
			fi

			if [ $counterIncreased -eq 1 ]; then
				alarmNotificationPeriod=$((60 * 15))		#15 minuti

				#fileName: 3267084_118879_2023-03-11-16-50-24.liveProxy.0.log
				array=(${fileName//_/ })
				ingestionJobKey=${array[0]}

				notify "$(hostname)" "alarm_${filterName}" "alarm_${filterName}_${fileName}" $alarmNotificationPeriod "got ${filterCount} times on file $logFile, ingestionJobKey: $ingestionJobKey"
				status=$?
				if [ $status -eq 0 ]; then
					#status 0 means alarm was sent
					echo "$filterCount" > $infoPathFileName
				fi
			fi
		fi
	done
}

mms_call_api_service()
{
	apiURL=$1
	basicAuthentication=$2

	if [ "$apiURL" = "" -o "$basicAuthentication" = "" ]
	then	
		apiURL="https://mms-api.catramms-cloud.com/catramms/1.0.1/mediaItem?start=0&rows=50"
		basicAuthentication="MTQ6RzNpM0s2bEkxUUdTU1lkVFIxVGQ2QllDMU9EeGZ4a3RoNnI1V1k1ZXpVRHR+ZUd+S2ZmdURGbncxc1ZCNkEzZA=="
	fi

	maxTime=5

	curlOptions_1="--silent --output /dev/null -w %{http_code} --max-time $maxTime"
	curlOptions_2="-X GET \"$apiURL\" -H \"accept:: application/json\" -H \"Authorization: Basic $basicAuthentication\""

	start=$(date +%s)
	httpStatus=$(curl $curlOptions_1 -X 'GET' "$apiURL" -H 'accept:: application/json' -H "Authorization: Basic $basicAuthentication")
	end=$(date +%s)

	if [ $httpStatus -eq 200 -a $((end-start)) -lt $maxTime ]; then
		echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_mms_call_api_service, mms call api is fine: curl $curlOptions_2" >> $debugFilename

		alarmNotificationPathFileName="/tmp/alarm_mms_call_api_service"
		if [ -f "$alarmNotificationPathFileName" ]; then
			rm -f $alarmNotificationPathFileName
		fi

		return 0
	else
		echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_mms_call_api_service, mms call api failed. httpStatus: $httpStatus, end-start: $((end-start)), curl $curlOptions_2" >> $debugFilename

		alarmNotificationPeriod=$((60 * 30))		#30 minuti
		if [ $((end-start)) -ge $maxTime ]; then
			alarmDetails="mms call api service too slow"
		else
			alarmDetails="mms call api service not working"
		fi
		notify "$(hostname)" "alarm_mms_call_api_service" "alarm_mms_call_api_service" $alarmNotificationPeriod "$alarmDetails"
		return 1
	fi
}

mms_api_timing_check_service()
{
	lastLogTimestampCheckedFile=/tmp/alarm_mms_api_timing_check_service_info

	if [ -f "$lastLogTimestampCheckedFile" ]; then
		lastLogTimestampChecked=$(cat $lastLogTimestampCheckedFile)
	else
		lastLogTimestampChecked=-1
	fi

	maxAPIDuration=2
	warningMessage=$(grep "manageRequestAndResponse, _requestIdentifier" /var/catramms/logs/mmsAPI/mmsAPI.log | awk -v lastLogTimestampChecked=$lastLogTimestampChecked -v lastLogTimestampCheckedFile=$lastLogTimestampCheckedFile -v maxAPIDuration=$maxAPIDuration 'BEGIN { FS="@"; newLastLogTimestampChecked=-1; } { datespec=substr($0, 2, 4)" "substr($0, 7, 2)" "substr($0, 10, 2)" "substr($0, 13, 2)" "substr($0, 16, 2)" "substr($0, 19, 2); newLastLogTimestampChecked=mktime(datespec); if(lastLogTimestampChecked == -1 || newLastLogTimestampChecked > lastLogTimestampChecked) { datetime=substr($0, 2, 23); method=$4; duration=$8; if (duration > maxAPIDuration) warningMessage=warningMessage""datetime" - "method" - "duration"\n"; } } END { printf("%s", warningMessage); printf("%s", newLastLogTimestampChecked) > lastLogTimestampCheckedFile; } ')

	if [ "$warningMessage" = "" ]; then
		echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_mms_api_timing_check_service, mms api timing is fine" >> $debugFilename

		alarmNotificationPathFileName="/tmp/alarm_mms_api_timing_check_service"
		if [ -f "$alarmNotificationPathFileName" ]; then
			rm -f $alarmNotificationPathFileName
		fi

		return 0
	else
		echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_mms_api_timing_check_service. warningMessage: $warningMessage" >> $debugFilename

		alarmNotificationPeriod=$((60 * 5))		#5 minuti
		notify "$(hostname)" "alarm_mms_api_timing_check_service" "alarm_mms_api_timing_check_service" $alarmNotificationPeriod "$warningMessage"
		return 1
	fi
}

