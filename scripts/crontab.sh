#!/bin/bash

export CatraMMS_PATH=/opt/catramms

#Retention (3 days: 4320 mins, 1 day: 1440 mins, 12 ore: 720 mins)
oneHourInMinutes=60
twelveHoursInMinutes=720
oneDayInMinutes=1440
twoDaysInMinutes=2880
threeDaysInMinutes=4320

if [ $# -ne 1 ]
then
    echo "$(date): usage $0 commandIndex" >> /tmp/crontab.log

    exit
fi

commandIndex=$1

if [ $commandIndex -eq 0 ]
then
	#update certificate
	sudo /usr/bin/certbot --quiet renew --pre-hook "$CatraMMS_PATH/CatraMMS/scripts/nginx.sh stop" --post-hook "$CatraMMS_PATH/CatraMMS/scripts/nginx.sh start"
else
	if [ $commandIndex -eq 1 ]
	then
		commandToBeExecuted="find /var/catramms/logs -mmin +$threeDaysInMinutes -type f -delete"
		timeoutValue="1h"
	elif [ $commandIndex -eq 2 ]
	then
		commandToBeExecuted="find /var/catramms/storage/MMSGUI/temporaryPushUploads -mmin +$threeDaysInMinutes -type f -delete"
		timeoutValue="1h"
	elif [ $commandIndex -eq 3 ]
	then
		commandToBeExecuted="find /var/catramms/storage/IngestionRepository/ -mmin +$threeDaysInMinutes -type f -delete"
		timeoutValue="1h"
	elif [ $commandIndex -eq 4 ]
	then
		commandToBeExecuted="find /var/catramms/storage/MMSWorkingAreaRepository/ -mmin +$twelveHoursInMinutes -type f -delete"
		timeoutValue="1h"
	elif [ $commandIndex -eq 5 ]
	then
		commandToBeExecuted="find /var/catramms/storage/MMSTranscoderWorkingAreaRepository/ffmpeg -mmin +$threeDaysInMinutes -type f -delete"
		timeoutValue="1h"
	elif [ $commandIndex -eq 6 ]
	then
		commandToBeExecuted="find /var/catramms/storage/MMSTranscoderWorkingAreaRepository/Staging -mmin +60 -type f -delete"
		timeoutValue="1h"
	elif [ $commandIndex -eq 7 ]
	then
		commandToBeExecuted="find /var/catramms/storage/DownloadRepository/* -empty -mmin +$threeDaysInMinutes -type d -delete"
		timeoutValue="1h"
	elif [ $commandIndex -eq 8 ]
	then
		commandToBeExecuted="find /var/catramms/storage/StreamingRepository/* -empty -mmin +$threeDaysInMinutes -type d -delete"
		timeoutValue="1h"
	elif [ $commandIndex -eq 9 ]
	then
		commandToBeExecuted="find /var/catramms/storage/MMSRepository/MMS_????/*/* -empty -mmin +$threeDaysInMinutes -type d -delete"
		timeoutValue="1h"
	elif [ $commandIndex -eq 10 ]
	then
		#2019-05-06: moved from 720 min to 360 min because we had the 'Argument list too long' error
		commandToBeExecuted="find /var/catramms/storage/MMSWorkingAreaRepository/Staging/* -empty -mmin +360 -type d -delete"
		timeoutValue="1h"
	elif [ $commandIndex -eq 11 ]
	then
		#2019-05-06: moved from 720 min to 360 min because we had the 'Argument list too long' error
		commandToBeExecuted="find /var/catramms/storage/MMSTranscoderWorkingAreaRepository/Staging/* -empty -mmin +360 -type d -delete"
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

		commandToBeEecuted="find /var/catramms/logs/nginx -mmin +$twoDaysInMinutes -type d -exec rm -rv {} +"
		timeoutValue="1h"
	elif [ $commandIndex -eq 13 ]
	then
		commandToBeExecuted="find /var/catramms/storage/MMSRepository-free/* -mmin +$oneHourInMinutes -type f -delete"
		timeoutValue="1h"
	elif [ $commandIndex -eq 14 ]
	then
		commandToBeExecuted="find /var/catramms/storage/MMSRepository-free/* -empty -mmin +$oneHourInMinutes -type d -delete"
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
		echo "$(date): $commandToBeEecuted FAILED (Argument list too long)" >> /tmp/crontab.log
	fi
fi

