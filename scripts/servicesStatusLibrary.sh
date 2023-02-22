#!/bin/bash

debugFilename=/tmp/servicesStatus.log
debug=1

if [ ! -f "$debugFilename" ]; then
	echo "" > $debugFilename
else
	filesize=$(stat -c %s $debugFilename)
	if [ $filesize -gt 1000000 ]
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
		"alarm_nginx_rate_gui_limit")
			echo "Nginx Rate GUI Limit overcome"
			;;
		"alarm_nginx_rate_api_limit")
			echo "Nginx Rate API Limit overcome"
			;;
		"alarm_nginx_rate_delivery_limit")
			echo "Nginx Rate Delivery Limit overcome"
			;;
		"alarm_nginx_rate_binary_limit")
			echo "Nginx Rate Binary Limit overcome"
			;;
		"alarm_nginx_rate_encoder_limit")
			echo "Nginx Rate Encoder Limit overcome"
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
	alarmNotificationPeriodInSeconds=$3
	alarmDetails=$4

	alarmNotificationPathFileName="/tmp/$alarmType"

	#controllo se è troppo presto per rimandare l'allarme
	if [ -f "$alarmNotificationPathFileName" ]; then
		lastNotificationTime=$(date -r "$alarmNotificationPathFileName" +%s)
		now=$(date +%s)
		elapsed=$((now-lastNotificationTime))
		if [ $elapsed -lt $alarmNotificationPeriodInSeconds ]; then
			echo "$(date +'%Y/%m/%d %H:%M:%S'): $alarmType not sent because too early, elapsed: $elapsed secs" >> $debugFilename
			return
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
			notify "$(hostname)" "alarm_sql_slave_off" $alarmNotificationPeriod ""
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
		notify "$(hostname)" "alarm_disks_usage" $alarmNotificationPeriod "$alarmsDiskUsage"
		return 1
	fi
}

cpu_usage()
{
	maxCpuUsage=50.0

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
		notify "$(hostname)" "alarm_cpu_usage" $alarmNotificationPeriod "${cpuUsage}%"
		return 1
	fi
}

memory_usage()
{
	maxMemoryUsage=50.0

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
		notify "$(hostname)" "alarm_memory_usage" $alarmNotificationPeriod "${memoryUsage}%"
		return 1
	fi
}

nginx_rate_encoder_limit()
{
	#aggiungo la data/ora come filtro altrimenti ritornerebbe sempre l'errore per tutto il giorno
	dateFilter=$(date +'%Y/%m/%d %H:')
	nginxRateLimitCount=$(grep "${dateFilter}" /var/catramms/logs/nginx/mms-encoder.error.log | grep "\"mmsEncoderLimit\"" | wc -l)

	if [ $nginxRateLimitCount -eq 0 ]; then
		echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_nginx_rate_encoder_limit, nginx rate Encoder limit is fine: $nginxRateLimitCount" >> $debugFilename

		alarmNotificationPathFileName="/tmp/alarm_nginx_rate_encoder_limit"
		if [ -f "$alarmNotificationPathFileName" ]; then
			rm -f $alarmNotificationPathFileName
		fi

		return 0
	else
		alarmNotificationPeriod=$((60 * 30))		#30 minuti
		notify "$(hostname)" "alarm_nginx_rate_encoder_limit" $alarmNotificationPeriod "overcame ${nginxRateLimitCount} times"
		return 1
	fi
}

nginx_rate_binary_limit()
{
	#aggiungo la data/ora come filtro altrimenti ritornerebbe sempre l'errore per tutto il giorno
	dateFilter=$(date +'%Y/%m/%d %H:')
	nginxRateLimitCount=$(grep "${dateFilter}" /var/catramms/logs/nginx/mms-binary.error.log | grep "\"mmsBinaryLimit\"" | wc -l)

	if [ $nginxRateLimitCount -eq 0 ]; then
		echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_nginx_rate_binary_limit, nginx rate Binary limit is fine: $nginxRateLimitCount" >> $debugFilename

		alarmNotificationPathFileName="/tmp/alarm_nginx_rate_binary_limit"
		if [ -f "$alarmNotificationPathFileName" ]; then
			rm -f $alarmNotificationPathFileName
		fi

		return 0
	else
		alarmNotificationPeriod=$((60 * 30))		#30 minuti
		notify "$(hostname)" "alarm_nginx_rate_binary_limit" $alarmNotificationPeriod "overcame ${nginxRateLimitCount} times"
		return 1
	fi
}

nginx_rate_delivery_limit()
{
	#aggiungo la data/ora come filtro altrimenti ritornerebbe sempre l'errore per tutto il giorno
	dateFilter=$(date +'%Y/%m/%d %H:')
	nginxRateLimitCount=$(grep "${dateFilter}" /var/catramms/logs/nginx/mms-delivery*.error.log | grep "\"mmsDeliveryLimit\"" | wc -l)

	if [ $nginxRateLimitCount -eq 0 ]; then
		echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_nginx_rate_delivery_limit, nginx rate Delivery limit is fine: $nginxRateLimitCount" >> $debugFilename

		if [ -f "$alarmNotificationPathFileName" ]; then
			rm -f $alarmNotificationPathFileName
		fi

		return 0
	else
		alarmNotificationPeriod=$((60 * 30))		#30 minuti
		notify "$(hostname)" "alarm_nginx_rate_delivery_limit" $alarmNotificationPeriod "${nginxRateLimitCount} times"
		return 1
	fi
}

nginx_rate_api_limit()
{
	#aggiungo la data/ora come filtro altrimenti ritornerebbe sempre l'errore per tutto il giorno
	dateFilter=$(date +'%Y/%m/%d %H:')
	nginxRateLimitCount=$(grep "${dateFilter}" /var/catramms/logs/nginx/mms-api.error.log | grep "\"mmsAPILimit\"" | wc -l)

	if [ $nginxRateLimitCount -eq 0 ]; then
		echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_nginx_rate_api_limit, nginx rate API limit is fine: $nginxRateLimitCount" >> $debugFilename

		alarmNotificationPathFileName="/tmp/alarm_nginx_rate_api_limit"
		if [ -f "$alarmNotificationPathFileName" ]; then
			rm -f $alarmNotificationPathFileName
		fi

		return 0
	else
		alarmNotificationPeriod=$((60 * 30))		#30 minuti
		notify "$(hostname)" "alarm_nginx_rate_api_limit" $alarmNotificationPeriod "overcame ${nginxRateLimitCount} times"
		return 1
	fi
}

nginx_rate_gui_limit()
{
	#aggiungo la data/ora come filtro altrimenti ritornerebbe sempre l'errore per tutto il giorno
	dateFilter=$(date +'%Y/%m/%d %H:')
	nginxRateLimitCount=$(grep "${dateFilter}" /var/catramms/logs/nginx/mms-gui.error.log | grep "\"mmsGUILimit\"" | wc -l)

	if [ $nginxRateLimitCount -eq 0 ]; then
		echo "$(date +'%Y/%m/%d %H:%M:%S'): alarm_nginx_rate_gui_limit, nginx rate GUI limit is fine: $nginxRateLimitCount" >> $debugFilename

		alarmNotificationPathFileName="/tmp/alarm_nginx_rate_gui_limit"
		if [ -f "$alarmNotificationPathFileName" ]; then
			rm -f $alarmNotificationPathFileName
		fi

		return 0
	else
		alarmNotificationPeriod=$((60 * 30))		#30 minuti
		notify "$(hostname)" "alarm_nginx_rate_gui_limit" $alarmNotificationPeriod "overcame ${nginxRateLimitCount} times"
		return 1
	fi
}

