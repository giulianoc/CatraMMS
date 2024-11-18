#!/bin/bash

debugFilename=/tmp/servicesStatus.log
debug=1

if [ ! -f "$debugFilename" ]; then 
  echo "" > $debugFilename
else 
  filesize=$(stat -c %s $debugFilename) 
  if [ $filesize -gt 50000000 ]; then 
    echo "" > $debugFilename 
  fi 
fi

getAlarmDescription()
{
	alarmType=$1

	case $alarmType in
		"alarm_sql_slave_off")
			echo "MYSQL SLAVE is not working"
			;;
		"alarm_postgres_replication_check")
			echo "Postgres replication is not working"
			;;
		"alarm_sql_check")
			echo "MYSQL is not working"
			;;
		"alarm_postgres_check")
			echo "Postgres is not working"
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
		"alarm_mount_error")
			echo "Mount Error"
			;;
		"alarm_nginx_error")
			echo "Nginx Error"
			;;
		"alarm_mms_service_running")
			echo "mms service is not running"
			;;
		"alarm_mms_call_api_service")
			echo "mms call api service not working or too slow"
			;;
		"alarm_mms_api_timing_check_service")
			echo "mms api too slow"
			;;
		"alarm_mms_webservices_timing_check_service")
			echo "mms_webservices api too slow"
			;;
		"alarm_mms_sql_timing_check_service")
			echo "mms sql too slow"
			;;
		"alarm_mms_server_reachable")
			echo "mms server is not reachable"
			;;
		*)
			echo "Unknown alarmType: $alarmType"
			echo "$(date +'%Y/%m/%d %H:%M:%S'): Unknown alarmType: $alarmType" >> $debugFilename
			;;
	esac
}

#getIngestionJobLabelByIngestionJobKey()
#{
#ingestionJobKey = $1

	##dominio da essere cambiato con catramms-cloud
    #apiIngestionJobDetailsURL="https://mms-api.cibortv-mms.com:443/catramms/1.0.1/ingestionJob/__INGESTIONJOBKEY__?ingestionJobOutputs=true&fromMaster=false"
	##custom key
    #basicAuthentication = "MTpITlZPb1ZoSHgweW9XTkl4RnUtVGhCQTF2QVBFS1dzeG5lR2d6ZTZlb2RkRXY5YUIxeHA5TnpzQktEQkRNRUZO"
#maxTime = 5
#apiIngestionJobDetailsURL = ${apiIngestionJobDetailsURL / __INGESTIONJOBKEY__ / $ingestionJobKey }
#ingestionJobKeyPathName = / tmp / $ingestionJobKey.json
#start = $(date + % s)
#curl - k-- silent-- output $ingestionJobKeyPathName-- max - time $maxTime - H 'accept:: application/json' -                                     \ H "Authorization: Basic $basicAuthentication" - X 'GET' "$apiIngestionJobDetailsURL"
#end = $(date + % s)
#ingestionJobLabel = ""
#if[-f "$ingestionJobKeyPathName"]; then
#fileSize = $(stat - c % s "$ingestionJobKeyPathName")
#if[$fileSize - gt 1000]; then
#ingestionJobLabel = $(cat $ingestionJobKeyPathName | jq '.response.ingestionJobs[0].label')
#echo                                                                                                                                                                                                   \ "$(date +'%Y/%m/%d %H:%M:%S'): getIngestionJobLabelByIngestionJobKey, apiIngestionJobDetailsURL: $apiIngestionJobDetailsURL, ingestionJobLabel: $ingestionJobLabel, elapsed: $((end-start)) secs">> \ $debugFilename
#else
#echo                                                                                                                                                                                                  \ "$(date +'%Y/%m/%d %H:%M:%S'): getIngestionJobLabelByIngestionJobKey, apiIngestionJobDetailsURL: $apiIngestionJobDetailsURL, $ingestionJobKeyPathName size: $(stat -c%s " $ingestionJobKeyPathName \ "), elapsed: $((end-start)) secs">> $debugFilename
#fi
#else
#echo                                                                                                                                                            \ "$(date +'%Y/%m/%d %H:%M:%S'): getIngestionJobLabelByIngestionJobKey, apiIngestionJobDetailsURL: $apiIngestionJobDetailsURL, elapsed: $((end-start)) secs">> \ $debugFilename
#fi
#rm - f $ingestionJobKeyPathName
#}

notify()
{
	serverName=$1
	alarmType=$2
	notifyFileName=$3
	alarmNotificationPeriodInSeconds=$4
	alarmDetails=$5
	customersToBeSentToo=$6

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

	curl -X POST	\
		--data-urlencode "chat_id=${TELEGRAM_GROUPALARMS_ID}" \
		--data-urlencode "text=${message}" \
		"https://api.telegram.org/bot${TELEGRAM_GROUPALARMS_BOT_TOKEN}/sendMessage" > /dev/null

	alarmNotificationIntegration="/home/mms/mms/scripts/alarmNotificationIntegration.sh"
	if [ -x "$alarmNotificationIntegration" ]; then
		echo "$(date +'%Y/%m/%d %H:%M:%S'): Calling $alarmNotificationIntegration" >> $debugFilename
		$alarmNotificationIntegration "$alarmType" "${message}" >> $debugFilename 2>&1
	fi

	return 0
}

mysql_sql_slave_off()
{
  #Questo controllo si applica solamente nel caso si tratta di uno slave

  #replication_connection_status.service_state(Slave_IO_Running) ON indica che è stato eseguito il comando SQL : start slave.
  #Nello scenario in cui abbiamo un problema, replication_connection_status.service_state rimane ON mentre
  #replication_applier_status_by_coordinator.service_state(Slave_SQL_Running) è OFF
  isMysqlSlave=$(echo "select service_state from performance_schema.replication_connection_status" | mysql -N -u ${DB_USER} -p${DB_PASSWORD} -h localhost ${DB_DBNAME})
  if [ "$isMysqlSlave" == "ON" ]; then

    #solo se è uno slave verifico il suo stato
	mysqlSlaveStatus=$(echo "select service_state from performance_schema.replication_applier_status_by_coordinator" | mysql -N -u ${DB_USER} -p${DB_PASSWORD} -h localhost ${DB_DBNAME})
	if [ "$mysqlSlaveStatus" == "ON" ]; then
		echo "$(date +'%Y/%m/%d %H:%M:%S'): sql_slave_off, slave is working fine" >> $debugFilename

		alarmNotificationPathFileName="/tmp/alarm_sql_slave_off"
		if [ -f "$alarmNotificationPathFileName" ]; then
			rm -f $alarmNotificationPathFileName
		fi

		return 0
	else
		alarmNotificationPeriod=$((60 * 15))		#15 minuti
		notify "$(hostname)" "alarm_sql_slave_off" "alarm_sql_slave_off" $alarmNotificationPeriod "isMysqlSlave: $isMysqlSlave, mysqlSlaveStatus: $mysqlSlaveStatus"
		return 1
	fi
  else
	echo "$(date +'%Y/%m/%d %H:%M:%S'): sql_slave_off, it is not a slave. isMysqlSlave: $isMysqlSlave" >> $debugFilename

	return 0
  fi
}

postgres_replication_check()
{
	isSlave=$(echo "select pg_is_in_recovery()" | psql --no-psqlrc -At "postgresql://${DB_USER}:${DB_PASSWORD}@postgres-localhost:5432/${DB_DBNAME}")
    #true(t) indica slave, false(f) indica master
	if [ "$isSlave" == "t" ]; then
      #in case of slave
	  status=$(echo "SELECT status FROM pg_stat_wal_receiver" | psql --no-psqlrc -At "postgresql://${DB_USER}:${DB_PASSWORD}@postgres-localhost:5432/${DB_DBNAME}")
	  if [ "$status" == "streaming" ]; then
			echo "$(date +'%Y/%m/%d %H:%M:%S'): postgres_replication_check, replication slave is working fine" >> $debugFilename

			alarmNotificationPathFileName="/tmp/alarm_postgres_replication_check"
			if [ -f "$alarmNotificationPathFileName" ]; then
				rm -f $alarmNotificationPathFileName
			fi

			return 0
	  else
			alarmNotificationPeriod=$((60 * 15))		#15 minuti
			notify "$(hostname)" "alarm_postgres_replication_check" "alarm_postgres_replication_check" $alarmNotificationPeriod "replication slave is not working, status: $status"
			return 1
	  fi
	elif [ "$isSlave" == "f" ]; then
      #in case of master
      #in questo caso avremo una riga per ogni slave connesso.In caso di funzionamento corretto,
      #il campo state di ogni riga deve essere 'streaming'
	  count=$(echo "SELECT count(*) FROM pg_stat_replication where state != 'streaming'" | psql --no-psqlrc -At "postgresql://${DB_USER}:${DB_PASSWORD}@postgres-localhost:5432/${DB_DBNAME}")
	  if [ $count -eq 0 ]; then
		echo "$(date +'%Y/%m/%d %H:%M:%S'): postgres_replication_check, replication master is working fine" >> $debugFilename

		alarmNotificationPathFileName="/tmp/alarm_postgres_replication_check"
		if [ -f "$alarmNotificationPathFileName" ]; then
			rm -f $alarmNotificationPathFileName
		fi

		return 0
	  else
			alarmNotificationPeriod=$((60 * 15))		#15 minuti
			notify "$(hostname)" "alarm_postgres_replication_check" "alarm_postgres_replication_check" $alarmNotificationPeriod "replication master: at least one communication with slave is not working, count: $count"
			return 1
	  fi
	else
		echo "$(date +'%Y/%m/%d %H:%M:%S'): postgres_replication_check, it is not a slave neither a master. isSlave: $isSlave" >> $debugFilename

		return 0
	fi
}

mysql_check()
{
	count=$(echo "select count(*) from MMS_Code" | mysql -N -u ${DB_USER} -p${DB_PASSWORD} -h localhost ${DB_DBNAME})

    #check if it is a number
	regularExpression='^[0-9]+$'
	if [[ $count =~ $regularExpression ]] ; then

        #it is a number

		echo "$(date +'%Y/%m/%d %H:%M:%S'): sql_check. sql is working fine" >> $debugFilename

		alarmNotificationPathFileName="/tmp/alarm_sql_check"
		if [ -f "$alarmNotificationPathFileName" ]; then
			rm -f $alarmNotificationPathFileName
		fi

		return 0
	else
		echo "$(date +'%Y/%m/%d %H:%M:%S'): sql_check. sql count return: $count" >> $debugFilename

		alarmNotificationPeriod=$((60 * 15))		#15 minuti
		notify "$(hostname)" "alarm_sql_check" "alarm_sql_check" $alarmNotificationPeriod "sql count return: $count"
		return 1
	fi
}

postgres_check()
{
	count=$(echo "select count(*) from MMS_TestConnection" | psql --no-psqlrc -At "postgresql://${DB_USER}:${DB_PASSWORD}@postgres-localhost:5432/${DB_DBNAME}")

    #check if it is a number
	regularExpression='^[0-9]+$'
	if [[ $count =~ $regularExpression ]] ; then

        #it is a number

		echo "$(date +'%Y/%m/%d %H:%M:%S'): postgres_check. postgres is working fine" >> $debugFilename

		alarmNotificationPathFileName="/tmp/alarm_postgres_check"
		if [ -f "$alarmNotificationPathFileName" ]; then
			rm -f $alarmNotificationPathFileName
		fi

		return 0
	else
		echo "$(date +'%Y/%m/%d %H:%M:%S'): postgres_check. postgres count return: $count" >> $debugFilename

		alarmNotificationPeriod=$((60 * 15))		#15 minuti
		notify "$(hostname)" "alarm_postgres_check" "alarm_postgres_check" $alarmNotificationPeriod "postgres count return: $count"
		return 1
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
	maxCpuUsage=70.0

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

mount_error()
{
	mountErrorFileName=/tmp/mount.error
	mountError=$(ls -la /mnt/ > /dev/null 2> $mountErrorFileName)
	mountErrorFileSize=$(stat -c%s "$mountErrorFileName")
	if [ $mountErrorFileSize = 0 ]; then
		echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_mount_error, mount is fine, mount error file size: $mountErrorFileSize" >> $debugFilename

		alarmNotificationPathFileName="/tmp/alarm_mount_error"
		if [ -f "$alarmNotificationPathFileName" ]; then
			rm -f $alarmNotificationPathFileName
		fi

		return 0
	else
		alarmNotificationPeriod=$((60 * 5))		#5 minuti
		notify "$(hostname)" "alarm_mount_error" "alarm_mount_error" $alarmNotificationPeriod "$(cat $mountErrorFileName)"
		return 1
	fi
}

nginx_error()
{
	serviceName=$1

#aggiungo la data / ora come filtro altrimenti ritornerebbe sempre l'errore per tutto il giorno
	dateFilter=$(date +'%Y/%m/%d %H:')
	nginxErrorsCount=$(grep "${dateFilter}" /var/catramms/logs/nginx/mms-${serviceName}.error.log | grep -v "No such file or directory" | grep -v "is forbidden" | grep -v "Stale file handle" | wc -l)

	if [ $nginxErrorsCount -eq 0 ]; then
		echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_nginx_error, nginx ${serviceName} is fine: $nginxErrorsCount" >> $debugFilename

		alarmNotificationPathFileName="/tmp/alarm_nginx_${serviceName}_error"
		if [ -f "$alarmNotificationPathFileName" ]; then
			rm -f $alarmNotificationPathFileName
		fi

		return 0
	else
		alarmNotificationPeriod=$((60 * 60))		#60 minuti
		notify "$(hostname)" "alarm_nginx_error" "alarm_nginx_${serviceName}_error" $alarmNotificationPeriod "service: ${serviceName}, got ${nginxErrorsCount} times"
		return 1
	fi
}

mms_service_running_by_processName()
{
	serviceName=$1
	processName=$2

	pgrep -f $processName > /dev/null
	serviceNotRunning=$?

	if [ $serviceNotRunning -eq 1 ]
	then
		alarmNotificationPeriod=$((60 * 1))		#1 minuti
		notify "$(hostname)" "alarm_mms_service_running" "alarm_mms_${serviceName}_service_running" $alarmNotificationPeriod ""

#fix management
		echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_mms_service_running, ${serviceName} service is restarted" >> $debugFilename

		~/mmsStopALL.sh
		sleep 1
		~/mmsStartALL.sh

		return 1
	else
		echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_mms_service_running, ${serviceName} is running" >> $debugFilename

		alarmNotificationPathFileName="/tmp/alarm_mms_${serviceName}_service_running"
		if [ -f "$alarmNotificationPathFileName" ]; then
			rm -f $alarmNotificationPathFileName
		fi

		return 0
	fi
}

mms_service_running_by_healthCheckURL()
{
	serviceName=$1
	healthCheckURL=$2

	outputHealthCheckURL=/tmp/mms_${serviceName}_service_running.healthCheckURL.response
	httpStatus=$(curl -k --output $outputHealthCheckURL -w "%{http_code}" --max-time 20 "$healthCheckURL")
	if [ $httpStatus -ne 200 ]
	then
		echo "$(date +'%Y/%m/%d %H:%M:%S'): mms_service_running failed, serviceName: ${serviceName}, healthCheckURL: $healthCheckURL, httpStatus: $httpStatus, outputHealthCheckURL: $(cat $outputHealthCheckURL)" >> $debugFilename

		failuresNumberFileName=/tmp/alarm_mms_${serviceName}_service_running.failuresNumber.txt
		if [ -s $failuresNumberFileName ]
		then
#exist and is not empty
			failuresNumber=$(cat $failuresNumberFileName)
		else
			failuresNumber=0
		fi
		maxFailuresNumber=5
		alarmNotificationPeriod=$((60 * 1))		#1 minuti
		notify "$(hostname)" "alarm_mms_service_running" "alarm_mms_${serviceName}_service_running" $alarmNotificationPeriod "serviceName: ${serviceName}, healthCheckURL: $healthCheckURL"

        #fix management
		if [ $failuresNumber -ge $maxFailuresNumber ]
		then
			echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_mms_service_running, ${serviceName} service is restarted" >> $debugFilename

			~/mmsStopALL.sh
			sleep 1
			~/mmsStartALL.sh

			rm -f $failuresNumberFileName
		else
			failuresNumber=$((failuresNumber+1))

			echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_mms_service_running, serviceName: ${serviceName}, one more failure: $failuresNumber, healthCheckURL: $healthCheckURL" >> $debugFilename

			echo "$failuresNumber" > $failuresNumberFileName
		fi

		return 1
	else
		echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_mms_service_running, mms ${serviceName} is running" >> $debugFilename

		alarmNotificationPathFileName="/tmp/alarm_mms_${serviceName}_service_running"
		if [ -f "$alarmNotificationPathFileName" ]; then
			rm -f $alarmNotificationPathFileName
		fi

        #fix management
		failuresNumberFileName=/tmp/alarm_mms_${serviceName}_service_running.failuresNumber.txt
		if [ -f "$failuresNumberFileName" ]; then
			rm -f $failuresNumberFileName
		fi

		return 0
	fi
}

ffmpeg_filter_detect()
{
	filterName=$1
	baseEncoderURL=$2
	encoderFilterNotificationURLUser=$3
	encoderFilterNotificationURLPassword=$4

    # 2880 : 2 giorni
	find /tmp -maxdepth 1 -name "alarm_${filterName}_*" -mmin +2880 -type f -delete

    folders=( /var/catramms/storage/MMSTranscoderWorkingAreaRepository/ffmpeg/liveRecorder_*.log /var/catramms/storage/MMSTranscoderWorkingAreaRepository/ffmpeg/liveProxy_*.log )
	for logFile in "${folders[@]}"
	do
		fileName=$(basename $logFile)
		filterCount=$(grep "$filterName" $logFile | wc -l)

		alarmNotificationPathFileName="/tmp/alarm_${filterName}_${fileName}"
		infoPathFileName="${alarmNotificationPathFileName}_info"
		if [ $filterCount -eq 0 ]; then
			echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_$filterName, filterCount: $filterCount, logFile: $logFile" >> $debugFilename

            # 2023 - 08 - 31 : remove non funzionante perchè, dopo che nel log saranno trovati i filtri, filterCount non sarà mai 0
            #Per questo motivo è stato aggiunto il find - delete sopra
            #if[-f "$alarmNotificationPathFileName"]; then
            #rm - f $alarmNotificationPathFileName
            #fi
            #if[-f "$infoPathFileName"]; then
            #rm - f $infoPathFileName
            #fi
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

                #fileName : liveProxy_6595960_1431305_2024 - 03 - 25 - 11 - 24 - 52.0.log
				array=(${fileName//_/ })
				ingestionJobKey=${array[1]}
				encodingJobKey=${array[2]}

                #http: // 10.0.1.7:8088/catramms/v1/encoder/filterNotification/6565093/1424493?filterName=freezeDetect
                encoderFilterNotificationURL="$baseEncoderURL/filterNotification/$ingestionJobKey/$encodingJobKey?filterName=$filterName"

	            maxTime=5
                start=$(date +%s)
                curl -k -u $encoderFilterNotificationURLUser:$encoderFilterNotificationURLPassword -w "%{http_code}" --silent --output /dev/null --max-time $maxTime -H 'accept:: application/json' -X 'GET' "$encoderFilterNotificationURL"
	            end=$(date +%s)
			    echo "$(date +'%Y/%m/%d %H:%M:%S'): encoderFilterNotificationURL: $encoderFilterNotificationURL, httpStatus: $httpStatus, filterCount: $filterCount, elapsed: $((end-start)) secs" >> $debugFilename
	            if [ $httpStatus -ne 200 ]
	            then
			       echo "$(date +'%Y/%m/%d %H:%M:%S'): encoderFilterNotificationURL failed: $encoderFilterNotificationURL, elapsed: $((end-start)) secs" >> $debugFilename
                fi

				##inizializza ingestionJobLabel
                #getIngestionJobLabelByIngestionJobKey $ingestionJobKey

                #notify "$(hostname)" "alarm_${filterName}" "alarm_${filterName}_${fileName}" $alarmNotificationPeriod "got ${filterCount} times on file $fileName, ingestionJob: $ingestionJobKey - $ingestionJobLabel"
                #status = $ ?
                #if[$status - eq 0]; then
                # #status 0 means alarm was sent
					echo "$filterCount" > $infoPathFileName
                #fi
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

	maxAPIDuration=2000
	warningMessage=$(grep "manageRequestAndResponse, _requestIdentifier" /var/catramms/logs/mmsAPI/mmsAPI.log | awk -v lastLogTimestampChecked=$lastLogTimestampChecked -v lastLogTimestampCheckedFile=$lastLogTimestampCheckedFile -v maxAPIDuration=$maxAPIDuration ' \
      BEGIN { FS="@"; newLastLogTimestampChecked=-1; }  \
      { \
        datespec=substr($0, 2, 4)" "substr($0, 7, 2)" "substr($0, 10, 2)" "substr($0, 13, 2)" "substr($0, 16, 2)" "substr($0, 19, 2); \
        newLastLogTimestampChecked=mktime(datespec);  \
        if(lastLogTimestampChecked == -1 || newLastLogTimestampChecked > lastLogTimestampChecked) \
        { \
          datetime=substr($0, 2, 23); \
          method=$4;  \
          duration=$8;  \
			    maxDuration = maxAPIDuration;	\
			    // custom max duration
			    if (method == "killOrCancelEncodingJob"	\
			    )	\
				    maxDuration = 10000;	\
			    else if (method == "uploadedBinary"	\
			    )	\
				    maxDuration = 3000;	\
          if (duration > maxDuration)  \
            warningMessage=warningMessage""datetime" - "method" - "duration"/"maxAPIDuration"\n";  \
        } \
    } \
    END \
    { \
      printf("%s", warningMessage); \
      printf("%s", newLastLogTimestampChecked) > lastLogTimestampCheckedFile;  \
    }  \
  ')

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

mms_webservices_timing_check_service()
{
	logDirName=$1
	logFileName=$2

	lastLogTimestampCheckedFile=/tmp/alarm_mms_webservices_${logFileName}_timing_check_service_info

	if [ -f "$lastLogTimestampCheckedFile" ]; then
		lastLogTimestampChecked=$(cat $lastLogTimestampCheckedFile)
	else
		lastLogTimestampChecked=-1
	fi

	maxAPIDuration=1000
	warningMessage=$(grep "API statistics" /var/catramms/logs/$logDirName/$logFileName.log | awk -v lastLogTimestampChecked=$lastLogTimestampChecked -v lastLogTimestampCheckedFile=$lastLogTimestampCheckedFile -v maxAPIDuration=$maxAPIDuration 'BEGIN { FS="@"; newLastLogTimestampChecked=-1; }	\
	{	\
		datespec=substr($0, 2, 4)" "substr($0, 7, 2)" "substr($0, 10, 2)" "substr($0, 13, 2)" "substr($0, 16, 2)" "substr($0, 19, 2);	\
		newLastLogTimestampChecked=mktime(datespec);	\
		if(lastLogTimestampChecked == -1 || newLastLogTimestampChecked > lastLogTimestampChecked) {	\
			datetime=substr($0, 2, 23);	\
			method=$2;	\
			duration=$4;	\
			otherInfo=$5;	\
			maxDuration = maxAPIDuration;	\
			// custom max duration
			if (method == "startChannels"	\
				|| method == "checkChannels"	\
			)	\
				maxDuration = 15000;	\
			else if (method == "epgUpdate"	\
			)	\
				maxDuration = 150000;	\
			else if (method == "promoList"	\
			)	\
				maxDuration = 1500;	\
			if (duration > maxDuration)	\
				warningMessage=warningMessage""datetime" - "method" - "duration"/"maxDuration" - "otherInfo"\n";	\
		}	\
	}	\
	END { printf("%s", warningMessage); printf("%s", newLastLogTimestampChecked) > lastLogTimestampCheckedFile; } ')

	if [ "$warningMessage" = "" ]; then
		echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_mms_webservices_${logFileName}_timing_check_service, mms_webservices timing is fine" >> $debugFilename

		alarmNotificationPathFileName="/tmp/alarm_mms_webservices_${logFileName}_timing_check_service"
		if [ -f "$alarmNotificationPathFileName" ]; then
			rm -f $alarmNotificationPathFileName
		fi

		return 0
	else
		echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_mms_webservices_${logFileName}_timing_check_service. warningMessage: $warningMessage" >> $debugFilename

		alarmNotificationPeriod=$((60 * 5))		#5 minuti
		notify "$(hostname)" "alarm_mms_webservices_timing_check_service" "alarm_mms_webservices_timing_check_service" $alarmNotificationPeriod "$warningMessage"
		return 1
	fi
}

mms_sql_timing_check_service()
{
	serviceName=$1

	lastLogTimestampCheckedFile=/tmp/alarm_mms_sql_timing_check_service_info

	if [ -f "$lastLogTimestampCheckedFile" ]; then
		lastLogTimestampChecked=$(cat $lastLogTimestampCheckedFile)
	else
		lastLogTimestampChecked=-1
	fi

	if [ "$serviceName" == "engine" ]; then
		logFilePathName=/var/catramms/logs/mmsEngineService/mmsEngineService.log
	elif [ "$serviceName" == "api" ]; then
		logFilePathName=/var/catramms/logs/mmsAPI/mmsAPI.log
	else
		echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_mms_sql_timing_check_service, wrong serviceName: $serviceName" >> $debugFilename
		return 1
	fi

	#incrementato a 300 perchè capita di avere poco piu di 200 anche per query perfettamente indicizzate che impiegano al 99.9% 1 millisecs
	# e poco piu di 200 nello 0.1%
	maxSQLDuration=300
	warningMessage=$(grep "statement, sqlStatement" $logFilePathName | awk -v lastLogTimestampChecked=$lastLogTimestampChecked -v lastLogTimestampCheckedFile=$lastLogTimestampCheckedFile -v maxSQLDuration=$maxSQLDuration 'BEGIN { FS="@"; newLastLogTimestampChecked=-1; }	\
	{	\
		datespec=substr($0, 2, 4)" "substr($0, 7, 2)" "substr($0, 10, 2)" "substr($0, 13, 2)" "substr($0, 16, 2)" "substr($0, 19, 2);	\
		newLastLogTimestampChecked=mktime(datespec);	\
		if(lastLogTimestampChecked == -1 || newLastLogTimestampChecked > lastLogTimestampChecked) {	\
			datetime=substr($0, 2, 23);	\
			sqlStatement=$2;	\
			duration=$6;	\
			label=$7;	\
			if (label == "getIngestionsToBeManaged")	\
				maxSQLDuration = 1200;	\
			else if (label == "getIngestionRootsStatus")	\
				maxSQLDuration = 400;	\
			if (duration > maxSQLDuration)	\
				warningMessage=warningMessage""datetime" - "label" - "sqlStatement" - "duration"/"maxSQLDuration"\n";	\
		}	\
	}	\
	END { printf("%s", warningMessage); printf("%s", newLastLogTimestampChecked) > lastLogTimestampCheckedFile; } ')

	if [ "$warningMessage" = "" ]; then
		echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_mms_sql_timing_check_service, mms sql ($serviceName) timing is fine" >> $debugFilename

		alarmNotificationPathFileName="/tmp/alarm_mms_sql_timing_check_service"
		if [ -f "$alarmNotificationPathFileName" ]; then
			rm -f $alarmNotificationPathFileName
		fi

		return 0
	else
		echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_mms_sql_timing_check_service. warningMessage: $warningMessage" >> $debugFilename

		alarmNotificationPeriod=$((60 * 5))		#5 minuti
		notify "$(hostname)" "alarm_mms_sql_timing_check_service" "alarm_mms_sql_timing_check_service" $alarmNotificationPeriod "$warningMessage"
		return 1
	fi
}

server_reachable()
{
	ip_address=$1
	port=$2
	host_name=$3

	if nc -w 15 -z $ip_address $port 2>/dev/null; then
		echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_mms_server_reachable, mms server is reachable: ip_address $ip_address, host_name: $host_name" >> $debugFilename

		alarmNotificationPathFileName="/tmp/alarm_mms_server_reachable"
		if [ -f "$alarmNotificationPathFileName" ]; then
			rm -f $alarmNotificationPathFileName
		fi

		return 0
	else
		echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_mms_server_reachable, mms server IS NOT reachable: ip_address $ip_address, host_name: $host_name" >> $debugFilename

		alarmNotificationPeriod=$((60 * 5))		#5 minuti
		alarmDetails="The $host_name ($ip_address) mms server IS NOT reachable"
		notify "$(hostname)" "alarm_mms_server_reachable" "alarm_mms_server_reachable_$host_name" $alarmNotificationPeriod "$alarmDetails"
		return 1
	fi
}

