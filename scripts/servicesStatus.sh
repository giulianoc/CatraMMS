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
	message="$(date +'%Y/%m/%d %H:%M:%S') - ${serverName} - ${alarmDescription}"

	curl -X GET	\
		--data-urlencode "chat_id=${TELEGRAM_GROUPALARMS_ID}" \
		--data-urlencode "text=${message}" \
		"https://api.telegram.org/bot${TELEGRAM_GROUPALARMS_BOT_TOKEN}/sendMessage" > /dev/null
}

sql_slave_off()
{
	isMysqlSlave=$(echo "select service_state from performance_schema.replication_connection_status" | mysql -N -u ${MYSQL_USER} -p${MYSQL_PASSWORD} -h localhost ${MYSQL_DBNAME})
	if [ "$isMysqlSlave" == "ON" ]; then
		#solo se è uno slave verifico il suo stato
		mysqlSlaveStatus=$(echo "select service_state from performance_schema.replication_applier_status_by_coordinator" | mysql -N -u ${MYSQL_USER} -p${MYSQL_PASSWORD} -h localhost ${MYSQL_DBNAME})
		if [ "$mysqlSlaveStatus" == "ON" ]; then
			alarmNotificationPathFileName="/tmp/alarm_sql_slave_off"
			if [ -f "$alarmNotificationPathFileName" ]; then
				rm -f $alarmNotificationPathFileName
			fi

			return 0
		else
			alarmNotificationPeriod=$((60 * 15))		#15 minuti
			notify "$(hostname)" "alarm_sql_slave_off" $alarmNotificationPeriod
			return 1
		fi
	else
		return 0
	fi
}

disks_usage()
{
	alarmsDiskUsage=$(df -Ph | awk 'BEGIN { alarms=""; maxDiskUsage=70; } { if (NR == 1) next; usagePercentage=substr($5, 0, length($5)-1)+0; if (usagePercentage > maxDiskUsage) { alarms=alarms$6" -> "usagePercentage"%; "; } } END {printf("%s", alarms) } ')
	if [ "$alarmsDiskUsage" == "" ]; then
		alarmNotificationPathFileName="/tmp/alarm_disks_usage"
		if [ -f "$alarmNotificationPathFileName" ]; then
			rm -f $alarmNotificationPathFileName
		fi

		return 0
	else
		alarmNotificationPeriod=$((60 * 15))		#15 minuti
		notify "$(hostname)" "alarm_disks_usage" $alarmNotificationPeriod
		return 1
	fi
}


if [ $# -ne 1 ]
then
	echo "usage $0 <moduleType (engine or api or encoder or externalEncoder or storage or integration)>" >> $debugFilename
	#echo "usage $0 <moduleType (load-balancer or engine or api or encoder or externalEncoder or storage or integration)>" >> $debugFilename

	exit
fi

moduleType=$1


disks_usage

if [ "$moduleType" == "engine" ]; then
	sql_slave_off
fi


