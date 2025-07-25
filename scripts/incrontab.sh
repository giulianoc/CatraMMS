#!/bin/bash

debug=1
#2021-01-08: it has to be a path in home user otherwise incron does not run the script
debugFileName=/home/mms/incrontab.log

if [ ! -f "$debugFileName" ]; then
	echo "" > $debugFileName
else
	filesize=$(stat -c %s $debugFileName)
	if [ $filesize -gt 10000000 ]
	then    
		echo "" > $debugFileName
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


MAX_PARALLEL=5											  	# Max sincronizzazioni in parallelo
BW_LIMIT=5000  											  	# Banda limite per ogni rsync (KB/s), opzionale
storageServer=116.202.53.105 						# mms-delivery-binary-gui-2
# srv-2.cibortvlive.com (storage USA), srv-10.cibortvlive.com (storage EU)
externalDeliveryServers="91.222.174.119 195.160.222.54"


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
	#date >> $debugFileName
	#whoami >> $debugFileName

	echo "$(date): $eventName --> $fileName ($channelDirectory)" >> $debugFileName

	if [[ "$fileName" == *.tmp ]]
	then
		#echo "$(date): temporary file: $eventName --> $fileName" >> $debugFileName
		#echo "" >> $debugFileName

		exit 0
	fi
fi

#Abbiamo:
#IN_MOVED_TO per i file .m3u8
#IN_MODIFY per i files .ts e .m3u8.tmp
#IN_DELETE per i file .ts
#IN_MOVED_FROM per i files .m3u8.tmp
#IN_CREATE per i files .m3u8.tmp e .ts
#we synchronize when .m3u8 is changed
if [ $eventName == "IN_MOVED_TO" ]
then
	if [ $debug -eq 1 ]
	then
		#Warning: Permanently added '[116.202.53.105]:9255' (ED25519) to the list of known hosts.
		#Warning: Permanently added '[194.42.206.8]:9255' (ED25519) to the list of known hosts.
		export channelDirectory BW_LIMIT debugFileName
		parallel --env channelDirectory --env BW_LIMIT --env debugFileName --jobs "$MAX_PARALLEL" --bar --halt now,fail=1 \
			'{
					echo "$(date) ({}): Inizio sincronizzazione...$(dirname $channelDirectory)";
					rsync -e "ssh -p 9255 -i ~/ssh-keys/hetzner-mms-key.pem -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no" \
						--delete --partial --progress --archive --verbose --omit-dir-times --timeout=60 --inplace --bwlimit=$BW_LIMIT \
						$channelDirectory mms@{}:$(dirname $channelDirectory)
					status=$?
					if [[ $status -eq 0 ]]; then
						echo "$(date) ({}): COMPLETATO" >> $debugFileName
					else
						echo "$(date) ({}): ERRORE: rsync exited with status $status" >> $debugFileName
					fi
				} >> $debugFileName 2>&1' \
				::: $storageServer $externalDeliveryServers
	else
		export channelDirectory BW_LIMIT debugFileName
		parallel --env channelDirectory --env BW_LIMIT --env debugFileName --jobs "$MAX_PARALLEL" --bar --halt now,fail=1 \
			'{
					rsync -e "ssh -p 9255 -i ~/ssh-keys/hetzner-mms-key.pem -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no" \
						--delete --partial --progress --archive --verbose --omit-dir-times --timeout=60 --inplace --bwlimit=$BW_LIMIT \
						$channelDirectory mms@{}:$(dirname $channelDirectory)
					status=$?
					if [[ $status -eq 0 ]]; then
						#echo "$(date) ({}): COMPLETATO" >> $debugFileName
					else
						echo "$(date) ({}): ERRORE: rsync exited with status $status" >> $debugFileName
					fi
				} >> $debugFileName 2>&1' \
				::: $storageServer $externalDeliveryServers
	fi
fi

if [ $debug -eq 1 ]
then
	echo "" >> $debugFileName
fi

