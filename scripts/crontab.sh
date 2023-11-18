#!/bin/bash

export CatraMMS_PATH=/opt/catramms

#Retention (3 days: 4320 mins, 1 day: 1440 mins, 12 ore: 720 mins)
oneHourInMinutes=60
sixHourInMinutes=360
twelveHoursInMinutes=720
oneDayInMinutes=1440
twoDaysInMinutes=2880
threeDaysInMinutes=4320
fiveDaysInMinutes=7200
tenDaysInMinutes=14400
twentyDaysInMinutes=28800
sixMonthsInMinutes=259299

debugFilename=/tmp/crontab.log
if [ ! -f "$debugFilename" ]; then
    echo "" > $debugFilename
else
    filesize=$(stat -c %s $debugFilename)
    if [ $filesize -gt 10000000 ]
    then
        echo "" > $debugFilename
    fi
fi

if [ $# -ne 1 -a $# -ne 2 -a $# -ne 3 ]
then
	echo "$(date): usage $0 <commandIndex> [<timeoutInMinutes>] [<db user> <db password>]" >> $debugFilename

    exit
fi

commandIndex=$1
timeoutInMinutes=$2
dbDetails=$3

if [ $commandIndex -eq 0 ]
then
	#update certificate

	#certbot path is different in case of ubuntu 18.04 or 20.04
	ubuntuVersion=$(cat /etc/lsb-release | grep -i RELEASE | cut -d'=' -f2 | cut -d'.' -f1)
	if [ $ubuntuVersion -eq 18 ]
	then
		sudo /usr/bin/certbot --quiet renew --pre-hook "$CatraMMS_PATH/CatraMMS/scripts/nginx.sh stop" --post-hook "$CatraMMS_PATH/CatraMMS/scripts/nginx.sh start"
	else
		export LD_LIBRARY_PATH=/opt/catramms/ffmpeg/lib && sudo certbot --quiet renew  --nginx-ctl /opt/catramms/nginx/sbin/nginx --nginx-server-root /opt/catramms/nginx/conf
	fi
else
	if [ $commandIndex -eq 1 ]
	then
		#first manage catalina.out file size if present
		file=/var/catramms/logs/tomcat-gui/catalina.out
		if [ -f "$file" ]
		then
			fileSizeInMegaBytes=$(du -m "$file" | awk '{print $1}')
			if [ $fileSizeInMegaBytes -gt 500 ]
			then
				echo "" > $file
			fi
		fi


		if [ "$timeoutInMinutes" == "" ]
		then
			timeoutInMinutes=$fiveDaysInMinutes
		fi

		commandToBeExecuted="find -L /var/catramms/logs/ -mmin +$timeoutInMinutes -type f -delete -print"
		timeoutValue="1h"
	elif [ $commandIndex -eq 2 ]
	then
		if [ "$timeoutInMinutes" == "" ]
		then
			timeoutInMinutes=$threeDaysInMinutes
		fi

		#serve per eliminare i file temporanei generati a causa di p:fileUpload non terminati.
		#Quelli che terminano vengono automaticamente eliminati.
		find /opt/catramms/tomcat/work/Catalina/localhost/catramms/ -mmin +$timeoutInMinutes -type f -delete -print >> $debugFilename

		commandToBeExecuted="find /var/catramms/storage/MMSGUI/temporaryPushUploads/ -mmin +$timeoutInMinutes -type f -delete -print"
		timeoutValue="1h"
	elif [ $commandIndex -eq 3 ]
	then
		if [ "$timeoutInMinutes" == "" ]
		then
			timeoutInMinutes=$threeDaysInMinutes
		fi

		# retention IngestionRepository for directories is nr. 8
		commandToBeExecuted="find /var/catramms/storage/IngestionRepository/ -mmin +$timeoutInMinutes -type f -delete -print"
		timeoutValue="1h"
	elif [ $commandIndex -eq 4 ]
	then
		if [ "$timeoutInMinutes" == "" ]
		then
			timeoutInMinutes=$twelveHoursInMinutes
		fi

		#2023-06-01: mantenere un numero di ore alto. Infatti, per video molto grandi,
		#	una volta terminati, serve molto tempo solo per fare la getMediaInfo e la move ed in questo periodo rimarrebbero qui
		#	senza cambiamenti e quindi, con 1h o 2h come timeout sarebbero eliminati prima di terminare la getMediaInfo/move causando un errore
		commandToBeExecuted="find /var/catramms/storage/MMSWorkingAreaRepository/ -mmin +$timeoutInMinutes -type f -delete -print"
		timeoutValue="1h"
	elif [ $commandIndex -eq 5 ]
	then
		if [ "$timeoutInMinutes" == "" ]
		then
			timeoutInMinutes=$threeDaysInMinutes
		fi

		#in ffmpegEndlessRecursivePlaylist troviamo i files (source + playlist) trasmessi come VODProxy
		#	- source files solo in caso di external encoder
		#	- playlist files sia per encoder interni che esterni
		#6 mesi di timeout potrebbero non essere sufficienti, spesso si esegue il VODProxy per anni!!!
		#Bisogna anche dire che proabilmente, per ogni nuovo deploy dell'encoder, questi files vengono riscritti.
		#Commentiamo per ora
		#find /var/catramms/storage/MMSTranscoderWorkingAreaRepository/ffmpegEndlessRecursivePlaylist/ -mmin +$sixMonthsInMinutes -type f -delete -print >> $debugFilename

		commandToBeExecuted="find /var/catramms/storage/MMSTranscoderWorkingAreaRepository/ffmpeg/ -mmin +$timeoutInMinutes -type f -delete -print"
		timeoutValue="1h"
	elif [ $commandIndex -eq 6 ]
	then
		if [ "$timeoutInMinutes" == "" ]
		then
			#2022-05-26: cambiato da 1 hour a 1 day because, in case of virtualVOD
			#of 2 hours, it was removing the segments when they were still in playlist
			timeoutInMinutes=$oneDayInMinutes
		fi

		commandToBeExecuted="find /var/catramms/storage/MMSTranscoderWorkingAreaRepository/Staging/ -mmin +$timeoutInMinutes -type f -delete -print"
		timeoutValue="1h"
	#2023-06-01: eliminata perchè già presente con commandIndex 4. Inoltre oneHour è troppo poco, per video molto grandi,
	#	una volta terminati, serve molto tempo solo per fare la getMediaInfo e la move ed in questo periodo rimarrebbero qui
	#	senza cambiamenti e quindi sarebbero eliminati prima di terminare la getMediaInfo/move causando un errore
	#elif [ $commandIndex -eq 7 ]
	#then
	#	if [ "$timeoutInMinutes" == "" ]
	#	then
	#		timeoutInMinutes=$oneHourInMinutes
	#	fi

	#	commandToBeExecuted="find /var/catramms/storage/MMSWorkingAreaRepository/Staging/ -mmin +$timeoutInMinutes -type f -delete -print"
	#	timeoutValue="1h"
	elif [ $commandIndex -eq 8 ]
	then
		if [ "$timeoutInMinutes" == "" ]
		then
			timeoutInMinutes=$threeDaysInMinutes
		fi

		# retention IngestionRepository for files is nr. 3
		commandToBeExecuted="find /var/catramms/storage/IngestionRepository/users/*/* -empty -mmin +$timeoutInMinutes -type d -delete -print"
		timeoutValue="1h"
	elif [ $commandIndex -eq 9 ]
	then
		if [ "$timeoutInMinutes" == "" ]
		then
			timeoutInMinutes=$sixHourInMinutes
		fi

		commandToBeExecuted="find /var/catramms/storage/MMSRepository/MMS_????/*/* -empty -mmin +$timeoutInMinutes -type d -delete -print"
		timeoutValue="1h"
	elif [ $commandIndex -eq 10 ]
	then
		if [ "$timeoutInMinutes" == "" ]
		then
			timeoutInMinutes=$oneDayInMinutes
		fi

		#elimina le directory in staging vuote
		#2019-05-06: moved from 720 min to 360 min because we had the 'Argument list too long' error
		#2023-06-12: rimosso .../Staging/*, in questo modo non avremo piu l'errore 'Argument list too long'
		#	Per evitare che il comando rimuove anche la dir Staging, aggiunto -not -path
		#	Incrementato il timeout per evitare che accade il seguente scenario
		#	- creato un Task Recording che ha aspettato diverse ore prima di ricevere il flusso di streaming
		#	- a causa di un piccolo timeout qui, la directory in MMSWorkingAreaRepository/Staging viene rimossa
		#	- quando finalmente è arrivato il flusso di streaming, la copia dei chunks in Staging falliva perchè
		#		non esisteva piu la directory (rimossa dal comando find ... -delete)
		commandToBeExecuted="find /var/catramms/storage/MMSWorkingAreaRepository/Staging -not -path /var/catramms/storage/MMSWorkingAreaRepository/Staging -empty -mmin +$timeoutInMinutes -type d -delete -print"
		timeoutValue="1h"
	elif [ $commandIndex -eq 11 ]
	then
		if [ "$timeoutInMinutes" == "" ]
		then
			#2022-05-26: cambiato da 1 hour a 1 day because, in case of virtualVOD
			#of 2 hours, it was removing the segments when they were still in playlist
			timeoutInMinutes=$oneDayInMinutes
		fi

		#2019-05-06: moved from 720 min to 360 min because we had the 'Argument list too long' error
		commandToBeExecuted="find /var/catramms/storage/MMSTranscoderWorkingAreaRepository/Staging/* -empty -mmin +$timeoutInMinutes -type d -delete -print"
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
			#sudo kill -USR1 $(cat /var/catramms/pids/nginx.pid)
			kill -USR1 $(cat /var/catramms/pids/nginx.pid)
		fi

		if [ "$timeoutInMinutes" == "" ]
		then
			timeoutInMinutes=$tenDaysInMinutes
		fi

		commandToBeExecuted="find /var/catramms/logs/nginx/ -mmin +$timeoutInMinutes -type d -print -exec rm -rv {} +"
		timeoutValue="1h"
	elif [ $commandIndex -eq 13 ]
	then
		if [ "$timeoutInMinutes" == "" ]
		then
			timeoutInMinutes=$oneDayInMinutes
		fi

		commandToBeExecuted="find /var/catramms/storage/MMSRepository/MMSLive/* -mmin +$timeoutInMinutes -type f -delete -print"
		timeoutValue="1h"
	elif [ $commandIndex -eq 14 ]
	then
		if [ "$timeoutInMinutes" == "" ]
		then
			timeoutInMinutes=$oneDayInMinutes
		fi

		commandToBeExecuted="find /var/catramms/storage/MMSRepository/MMSLive/* -empty -mmin +$timeoutInMinutes -type d -delete -print"
		timeoutValue="1h"
	elif [ $commandIndex -eq 15 ]
	then
		if [[ "$timeoutInMinutes" == "" || "$timeoutInMinutes" == "0" ]]
		then
			timeoutInMinutes=$tenDaysInMinutes
		fi

		timeoutValue="1h"

		#2020-12-09: added / at the end of dumpDirectory (because it is a link,
		#'find' would not work)
		#dumpDirectory=/var/catramms/storage/dbDump/
		arrayOfDBUserPwd=($dbDetails)
		dbUserPwdNumber="${#arrayOfDBUserPwd[@]}"
		dbUserPwdIndex=0
		#echo $dbUserPwdNumber
		while [[ $dbUserPwdIndex -lt $dbUserPwdNumber ]]; do
			dbUser=${arrayOfDBUserPwd[$dbUserPwdIndex]}
			dbPwd=${arrayOfDBUserPwd[$((dbUserPwdIndex+1))]}
			dbName=${arrayOfDBUserPwd[$((dbUserPwdIndex+2))]}
			dumpDirectory=${arrayOfDBUserPwd[$((dbUserPwdIndex+3))]}

			if [ "$dbName" == "mms" ];then
				dumpFileName=${dbUser}_$(date +"%Y-%m-%d")_forSlave.sql
				postgresDumpFileName=postgres_${dbUser}_$(date +"%Y-%m-%d").sql
			else
				dumpFileName=${dbUser}_$(date +"%Y-%m-%d").sql
			fi
			#echo $dbUser $dbPwd $dbName $dumpFileName
			#--dump-replica: esegue STOP SLAVE, fa il dump e alla fine esegue START SLAVE. Aggiunge nel dump CHANGE REPLICATION SOURCE TO (per quando questo file viene importato dallo slave)
			#--apply-replica-statements: aggiunge nel dump STOP SLAVE and START SLAVE (per quando questo file viene importato dallo slave)
			#--include-source-host-port: aggiunge server and port nel comando di CHANGE REPLICATION SOURCE TO
			#dbUser deve avere i diritti per eseguire SHOW REPLICA STATUS
			if [ "$dbName" == "mms" ];then
				mysqldump --no-tablespaces --dump-replica --apply-replica-statements --include-source-host-port -u $dbUser -p$dbPwd -h db-slaves $dbName | gzip > $dumpDirectory$dumpFileName.gz # && gzip -f $dumpDirectory$dumpFileName
				pg_dump "postgresql://$dbUser:$dbPwd@postgres-slaves:5432/$dbName" --clean --if-exists | gzip > $dumpDirectory$postgresDumpFileName.gz
			else
				mysqldump --no-tablespaces -u $dbUser -p$dbPwd -h db-slaves $dbName | gzip > $dumpDirectory$dumpFileName.gz # && gzip -f $dumpDirectory$dumpFileName
			fi

			dbUserPwdIndex=$((dbUserPwdIndex+4))

			#the retention command is called here because in case of multiple DB
			#the command would be called only for the last one
			commandToBeExecuted="find $dumpDirectory -mmin +$timeoutInMinutes -type f -delete -print"
			timeout $timeoutValue $commandToBeExecuted
		done

	else
		echo "$(date): wrong commandIndex: $commandIndex" >> $debugFilename

		exit
	fi

	timeout $timeoutValue $commandToBeExecuted >> $debugFilename
	if [ $? -eq 124 ]
	then
		echo "$(date): $commandToBeExecuted TIMED OUT" >> $debugFilename
	elif [ $? -eq 126 ]
	then
		echo "$(date): $commandToBeExecuted FAILED (Argument list too long)" >> $debugFilename
	fi
fi

