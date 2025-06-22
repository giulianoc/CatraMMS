#!/bin/bash

debugFilename=/tmp/backupStorage.log

#lo eseguiamo una volta al giorno
#while [ 1 -eq 1 ]
#do
	if [ ! -f "$debugFilename" ]; then
		echo "" > $debugFilename
	else
		filesize=$(stat -c %s $debugFilename)
		if [ $filesize -gt 10000000 ]
		then
			echo "" > $debugFilename
		fi
	fi

	#--progress --verbose

	echo "" >> $debugFilename
	echo "" >> $debugFilename

	echo "$(date +'%Y/%m/%d %H:%M:%S'): start backup mmsRepository0000..." >> $debugFilename
	start=$(date +%s)
	#max 8h
	#timeout 480m rsync --verbose --archive --recursive --delete --partial /mnt/mmsStorage-1-new/mmsRepository0000/ /mnt/backupRepository0000/ >> $debugFilename
	timeout 480m rsync -e "ssh -p 9255 -i ~/.hetzner-mms-key.pem" --verbose --archive --recursive --delete --partial /mnt/mmsStorage-1-new/* mms@10.0.1.21:/mnt/storage-1-backup >> $debugFilename
	end=$(date +%s)
	elapsed=$((end-start))
	elapsedInMin=$(echo "scale=1; $elapsed / 60" | bc)
	echo "$(date +'%Y/%m/%d %H:%M:%S'): end backup mmsRepository0000, elapsed: $elapsedInMin min" >> $debugFilename

#	echo "$(date +'%Y/%m/%d %H:%M:%S'): backup (total) elapsed: $elapsedTotal secs, sleeping $secondsToSleep secs" >> $debugFilename
#	sleep $secondsToSleep
#done

