#!/bin/bash

eventName=$1
channelDirectory=$2
fileName=$3


storageServer=cibortv-mms-api-gui-1


#Example of events using debug:
#IN_CREATE --> 1258481.ts (/var/catramms/storage/MMSRepository/MMSLive/1/1258)
#IN_MODIFY --> 1258481.ts (/var/catramms/storage/MMSRepository/MMSLive/1/1258)
#IN_MODIFY --> 1258481.ts (/var/catramms/storage/MMSRepository/MMSLive/1/1258)
#IN_MODIFY --> 1258481.ts (/var/catramms/storage/MMSRepository/MMSLive/1/1258)
#temporary file: IN_CREATE --> 1258.m3u8.tmp
#temporary file: IN_MODIFY --> 1258.m3u8.tmp
#temporary file: IN_MOVED_FROM --> 1258.m3u8.tmp
#IN_MOVED_TO --> 1258.m3u8 (/var/catramms/storage/MMSRepository/MMSLive/1/1258)

debug=0
#2021-01-08: it has to be a path in home user otherwise incron does not run the script
debugFileName=/home/mms/incrontab.log

if [ $debug -eq 1 ]
then
	date >> $debugFileName
	whoami >> $debugFileName

	if [[ "$fileName" == *.tmp ]]
	then
		echo "temporary file: $eventName --> $fileName" >> $debugFileName
		echo "" >> $debugFileName

		exit 0
	fi

	echo "$eventName --> $fileName ($channelDirectory)" >> $debugFileName
fi

#we synchronize when .m3u8 is changed
if [ $eventName == "IN_MOVED_TO" ]
then
	if [ $debug -eq 1 ]
	then
		echo "rsync -az -e \"ssh -p 9255\" --delete --partial --progress --archive --verbose --compress --omit-dir-times $channelDirectory mms@$storageServer:$(dirname $channelDirectory)" >> $debugFileName
		rsync -az -e "ssh -p 9255" --delete --partial --progress --archive --verbose --compress --omit-dir-times $channelDirectory mms@$storageServer:$(dirname $channelDirectory) >> $debugFileName 2>&1 
	else
		rsync -az -e "ssh -p 9255" --delete --partial --progress --archive --verbose --compress --omit-dir-times $channelDirectory mms@$storageServer:$(dirname $channelDirectory) > /dev/null
	fi
fi

if [ $debug -eq 1 ]
then
	echo "" >> $debugFileName
fi

