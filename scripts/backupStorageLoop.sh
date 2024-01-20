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

	before=$(date +%s)
	rsync --archive --recursive --compress --delete --partial --omit-dir-times --progress --verbose /mnt/mmsIngestionRepository-main/ /mnt/mmsIngestionRepository-backup >> $debugFilename
	rsync --archive --recursive --compress --delete --partial --omit-dir-times --progress --verbose /mnt/mmsStorage-main/ /mnt/mmsStorage-backup >> $debugFilename
	rsync --archive --recursive --compress --delete --partial --omit-dir-times  --progress --verbose /mnt/mmsRepository0000-main/ /mnt/mmsRepository0000-backup >> $debugFilename
	after=$(date +%s)

	elapsed=$((after-before))

	secondsToSleep=300

	echo "" >> $debugFilename
	echo "$(date +'%Y/%m/%d %H:%M:%S'): backup elapsed: $elapsed secs, sleeping $secondsToSleep secs" >> $debugFilename
	sleep $secondsToSleep
done

