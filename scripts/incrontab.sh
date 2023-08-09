#!/bin/bash

debug=0
#2021-01-08: it has to be a path in home user otherwise incron does not run the script
debugFileName=/home/mms/incrontab.log

if [ ! -f "$debugFilename" ]; then
	echo "" > $debugFilename
else
	filesize=$(stat -c %s $debugFilename)
	if [ $filesize -gt 10000000 ]
	then    
		echo "" > $debugFilename
	fi              
fi

if [ $# -ne 3 ]
then
	if [ $debug -eq 1 ]
	then
		echo "$(date): usage $0 <eventName> <channelDirectory> <fileName>" >> $debugFileName
	fi

	exit
fi

eventName=$1
channelDirectory=$2
fileName=$3


storageServer=mms-api-2.catramms-cloud.com


#Example of events using debug:
#IN_CREATE --> 1258481.ts (/var/catramms/storage/MMSRepository/MMSLive/1/1258)
#IN_MODIFY --> 1258481.ts (/var/catramms/storage/MMSRepository/MMSLive/1/1258)
#IN_MODIFY --> 1258481.ts (/var/catramms/storage/MMSRepository/MMSLive/1/1258)
#IN_MODIFY --> 1258481.ts (/var/catramms/storage/MMSRepository/MMSLive/1/1258)
#temporary file: IN_CREATE --> 1258.m3u8.tmp
#temporary file: IN_MODIFY --> 1258.m3u8.tmp
#temporary file: IN_MOVED_FROM --> 1258.m3u8.tmp
#IN_MOVED_TO --> 1258.m3u8 (/var/catramms/storage/MMSRepository/MMSLive/1/1258)


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
		echo "rsync -az -e \"ssh -p 9255 -i ~/ssh-keys/hetzner-mms-key.pem -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no\" --delete --partial --progress --archive --verbose --compress --omit-dir-times $channelDirectory mms@$storageServer:$(dirname $channelDirectory)" >> $debugFileName
		rsync -az -e "ssh -p 9255 -i ~/ssh-keys/hetzner-mms-key.pem -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no" --delete --partial --progress --archive --verbose --compress --omit-dir-times $channelDirectory mms@$storageServer:$(dirname $channelDirectory) >> $debugFileName 2>&1 
	else
		rsync -az -e "ssh -p 9255 -i ~/ssh-keys/hetzner-mms-key.pem -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no" --delete --partial --progress --archive --verbose --compress --omit-dir-times $channelDirectory mms@$storageServer:$(dirname $channelDirectory) > /dev/null
	fi
fi

if [ $debug -eq 1 ]
then
	echo "" >> $debugFileName
fi

