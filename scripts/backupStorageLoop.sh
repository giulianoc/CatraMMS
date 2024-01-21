#!/bin/bash

debugFilename=/tmp/backupStorage.log

while [ 1 -eq 1 ]
do
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

	startTotal=$(date +%s)

	echo "$(date +'%Y/%m/%d %H:%M:%S'): backup mmsIngestionRepository..." >> $debugFilename
	start=$(date +%s)
	rsync --archive --recursive --compress --delete --partial --omit-dir-times /mnt/mmsIngestionRepository-main/ /mnt/mmsIngestionRepository-backup >> $debugFilename
	end=$(date +%s)
	elapsed=$((end-start))
	echo "$(date +'%Y/%m/%d %H:%M:%S'): backup mmsIngestionRepository elapsed: $elapsed secs" >> $debugFilename

	echo "$(date +'%Y/%m/%d %H:%M:%S'):backup mmsStorage..." >> $debugFilename
	start=$(date +%s)
	rsync --archive --recursive --compress --delete --partial --omit-dir-times /mnt/mmsStorage-main/ /mnt/mmsStorage-backup >> $debugFilename
	end=$(date +%s)
	elapsed=$((end-start))
	echo "$(date +'%Y/%m/%d %H:%M:%S'): backup mmsStorage elapsed: $elapsed secs" >> $debugFilename

	echo "$(date +'%Y/%m/%d %H:%M:%S'): backup mmsRepository0000..." >> $debugFilename
	start=$(date +%s)
	rsync --archive --recursive --compress --delete --partial --omit-dir-times /mnt/mmsRepository0000-main/ /mnt/mmsRepository0000-backup >> $debugFilename
	end=$(date +%s)
	elapsed=$((end-start))
	echo "$(date +'%Y/%m/%d %H:%M:%S'): backup mmsRepository0000 elapsed: $elapsed secs" >> $debugFilename

	endTotal=$(date +%s)
	elapsedTotal=$((endTotal-startTotal))

	#3600 * 5 = 18000
	#60 * 15 = 900
	secondsToSleep=900

	echo "" >> $debugFilename
	echo "$(date +'%Y/%m/%d %H:%M:%S'): backup (total) elapsed: $elapsedTotal secs, sleeping $secondsToSleep secs" >> $debugFilename
	sleep $secondsToSleep
done

