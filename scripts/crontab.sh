#!/bin/bash

export CatraMMS_PATH=/opt/catramms

#Retention (3 days: 4320 mins, 1 day: 1440 mins, 12 ore: 720 mins)
oneHourInMinutes=60
sixHourInMinutes=360
twelveHoursInMinutes=720
oneDayInMinutes=1440
twoDaysInMinutes=2880
threeDaysInMinutes=4320

if [ $# -ne 1 -a $# -ne 2 ]
then
    echo "$(date): usage $0 <commandIndex> [<timeoutInMinutes>]" >> /tmp/crontab.log

    exit
fi

commandIndex=$1
timeoutInMinutes=$2

if [ $commandIndex -eq 0 ]
then
	#update certificate
	sudo /usr/bin/certbot --quiet renew --pre-hook "$CatraMMS_PATH/CatraMMS/scripts/nginx.sh stop" --post-hook "$CatraMMS_PATH/CatraMMS/scripts/nginx.sh start"
else
	if [ $commandIndex -eq 1 ]
	then
		#first manage catalina.out file size if present
		file=/var/catramms/logs/tomcat-gui/catalina.out
		if [ -f "$file" ]
		then
			fileSizeInMegaBytes=$(du -m "$file" | awk '{print $1}')
			if [ $fileSizeInMegaBytes -gt 1000 ]
			then
				echo "" > $file
			fi
		fi


		if [ "$timeoutInMinutes" == "" ]
		then
			timeoutInMinutes=$threeDaysInMinutes
		fi

		commandToBeExecuted="find /var/catramms/logs -mmin +$timeoutInMinutes -type f -delete"
		timeoutValue="1h"
	elif [ $commandIndex -eq 2 ]
	then
		if [ "$timeoutInMinutes" == "" ]
		then
			timeoutInMinutes=$threeDaysInMinutes
		fi

		commandToBeExecuted="find /var/catramms/storage/MMSGUI/temporaryPushUploads -mmin +$timeoutInMinutes -type f -delete"
		timeoutValue="1h"
	elif [ $commandIndex -eq 3 ]
	then
		if [ "$timeoutInMinutes" == "" ]
		then
			timeoutInMinutes=$threeDaysInMinutes
		fi

		commandToBeExecuted="find /var/catramms/storage/IngestionRepository/ -mmin +$timeoutInMinutes -type f -delete"
		timeoutValue="1h"
	elif [ $commandIndex -eq 4 ]
	then
		if [ "$timeoutInMinutes" == "" ]
		then
			timeoutInMinutes=$twelveHoursInMinutes
		fi

		commandToBeExecuted="find /var/catramms/storage/MMSWorkingAreaRepository/ -mmin +$timeoutInMinutes -type f -delete"
		timeoutValue="1h"
	elif [ $commandIndex -eq 5 ]
	then
		if [ "$timeoutInMinutes" == "" ]
		then
			timeoutInMinutes=$threeDaysInMinutes
		fi

		commandToBeExecuted="find /var/catramms/storage/MMSTranscoderWorkingAreaRepository/ffmpeg -mmin +$timeoutInMinutes -type f -delete"
		timeoutValue="1h"
	elif [ $commandIndex -eq 6 ]
	then
		if [ "$timeoutInMinutes" == "" ]
		then
			timeoutInMinutes=$oneHourInMinutes
		fi

		commandToBeExecuted="find /var/catramms/storage/MMSTranscoderWorkingAreaRepository/Staging -mmin +$timeoutInMinutes -type f -delete"
		timeoutValue="1h"
	elif [ $commandIndex -eq 7 ]
	then
		if [ "$timeoutInMinutes" == "" ]
		then
			timeoutInMinutes=$oneHourInMinutes
		fi

		commandToBeExecuted="find /var/catramms/storage/MMSWorkingAreaRepository/Staging -mmin +$timeoutInMinutes -type f -delete"
		timeoutValue="1h"
	elif [ $commandIndex -eq 8 ]
	then
		if [ "$timeoutInMinutes" == "" ]
		then
			timeoutInMinutes=$threeDaysInMinutes
		fi

		commandToBeExecuted="find /var/catramms/storage/StreamingRepository/* -empty -mmin +$timeoutInMinutes -type d -delete"
		timeoutValue="1h"
	elif [ $commandIndex -eq 9 ]
	then
		if [ "$timeoutInMinutes" == "" ]
		then
			timeoutInMinutes=$sixHourInMinutes
		fi

		commandToBeExecuted="find /var/catramms/storage/MMSRepository/MMS_????/*/* -empty -mmin +$timeoutInMinutes -type d -delete"
		timeoutValue="1h"
	elif [ $commandIndex -eq 10 ]
	then
		if [ "$timeoutInMinutes" == "" ]
		then
			timeoutInMinutes=$oneHourInMinutes
		fi

		#2019-05-06: moved from 720 min to 360 min because we had the 'Argument list too long' error
		commandToBeExecuted="find /var/catramms/storage/MMSWorkingAreaRepository/Staging/* -empty -mmin +$timeoutInMinutes -type d -delete"
		timeoutValue="1h"
	elif [ $commandIndex -eq 11 ]
	then
		if [ "$timeoutInMinutes" == "" ]
		then
			timeoutInMinutes=$oneHourInMinutes
		fi

		#2019-05-06: moved from 720 min to 360 min because we had the 'Argument list too long' error
		commandToBeExecuted="find /var/catramms/storage/MMSTranscoderWorkingAreaRepository/Staging/* -empty -mmin +$timeoutInMinutes -type d -delete"
		timeoutValue="1h"
	elif [ $commandIndex -eq 12 ]
	then
		DATE=$(date +%Y-%m-%d)
		DIRPATHNAME=/var/catramms/logs/nginx/$DATE
		if [ ! -d "$DIRPATHNAME" ]; then
			mkdir $DIRPATHNAME
			mv /var/catramms/logs/nginx/*.log $DIRPATHNAME

			#BE CAREFULL SUDO MAY ASK PASSWORD. 
			#Add the command '.../crontab.rsi.sh 12' to 'sudo crontab -e'
			sudo kill -USR1 $(cat /var/catramms/pids/nginx.pid)
		fi

		if [ "$timeoutInMinutes" == "" ]
		then
			timeoutInMinutes=$twoDaysInMinutes
		fi

		commandToBeExecuted="find /var/catramms/logs/nginx -mmin +$timeoutInMinutes -type d -exec rm -rv {} +"
		timeoutValue="1h"
	elif [ $commandIndex -eq 13 ]
	then
		if [ "$timeoutInMinutes" == "" ]
		then
			timeoutInMinutes=$oneHourInMinutes
		fi

		commandToBeExecuted="find /var/catramms/storage/MMSRepository-free/* -mmin +$timeoutInMinutes -type f -delete"
		timeoutValue="1h"
	elif [ $commandIndex -eq 14 ]
	then
		if [ "$timeoutInMinutes" == "" ]
		then
			timeoutInMinutes=$oneHourInMinutes
		fi

		commandToBeExecuted="find /var/catramms/storage/MMSRepository-free/* -empty -mmin +$timeoutInMinutes -type d -delete"
		timeoutValue="1h"
	else
		echo "$(date): wrong commandIndex: $commandIndex" >> /tmp/crontab.log

		exit
	fi

	timeout $timeoutValue $commandToBeExecuted
	if [ $? -eq 124 ]
	then
		echo "$(date): $commandToBeExecuted TIMED OUT" >> /tmp/crontab.log
	elif [ $? -eq 126 ]
	then
		echo "$(date): $commandToBeExecuted FAILED (Argument list too long)" >> /tmp/crontab.log
	fi
fi

