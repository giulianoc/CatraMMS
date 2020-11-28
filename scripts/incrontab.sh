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

if [ $debug -eq 1 ]
then
	date >> /tmp/change.txt
	whoami >> /tmp/change.txt

	if [[ "$fileName" == *.tmp ]]
	then
		echo "temporary file: $eventName --> $fileName" >> /tmp/change.txt
		echo "" >> /tmp/change.txt

		exit 0
	fi

	echo "$eventName --> $fileName ($channelDirectory)" >> /tmp/change.txt
fi

#we synchronize when .m3u8 is changed
if [ $eventName == "IN_MOVED_TO" ]
then
	if [ $debug -eq 1 ]
	then
		echo "rsync -az -e \"ssh -p 9255\" --delete --partial --progress --archive --verbose --compress --omit-dir-times $channelDirectory mms@$storageServer:$(dirname $channelDirectory)" >> /tmp/change.txt
		rsync -az -e "ssh -p 9255" --delete --partial --progress --archive --verbose --compress --omit-dir-times $channelDirectory mms@$storageServer:$(dirname $channelDirectory) >> /tmp/change.txt 2>&1 
	else
		rsync -az -e "ssh -p 9255" --delete --partial --progress --archive --verbose --compress --omit-dir-times $channelDirectory mms@$storageServer:$(dirname $channelDirectory) > /dev/null
	fi
fi

if [ $debug -eq 1 ]
then
	echo "" >> /tmp/change.txt
fi

