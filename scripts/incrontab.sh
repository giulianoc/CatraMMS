#!/bin/bash

debug=1
debug_rsync=0
#2021-01-08: it has to be a path in home user otherwise incron does not run the script
debugFileName=/home/mms/incrontab.log

start=$(date +%s)
pid=$$

if [ ! -f "$debugFileName" ]; then
	echo "" > $debugFileName
else
	filesize=$(stat -c %s $debugFileName)
	# 2 GB = 2 * 1024 * 1024 * 1024 = 2147483648 bytes
	if (( filesize > 2147483648 ))
	then
		echo "" > $debugFileName
	fi              
fi

if [ $# -ne 3 ]
then
	if [ $debug -eq 1 ]
	then
		echo "$(date)-$pid: usage $0 <eventName> <channelDirectory> <fileName>" >> $debugFileName
	fi

	exit
fi

eventName=$1
channelDirectory=$2
fileName=$3

# Lista degli IP (separati da spazio)
mmsStorageIPList=("116.202.53.105" "116.202.172.245") #mms-delivery-binary-gui-XXXXX
usaExternalDeliveriesIPList=("91.222.174.119")
euExternalDeliveriesIPList=("195.160.222.54")

get_next_ip() {
	local list_name="$1"                   # es: "mmsStorageIPList"
	# File che memorizza l'indice corrente
	local state_file="/tmp/incontab-round_robin_${list_name}_index"

 	# Crea un riferimento all'array con nome dinamico
 	# crea un nameref, cioè un riferimento a una variabile di nome contenuto in un’altra variabile.
	declare -n ip_list="$list_name"

	local total_ips=${#ip_list[@]}

	# Leggi indice corrente (default 0 se mancante)
	local current_index=0
	if [[ -f "$state_file" ]]; then
		current_index=$(<"$state_file")
	fi

	# Stampa IP corrente
	echo "${ip_list[$current_index]}"

	# Calcola il prossimo indice
	local next_index=$(( (current_index + 1) % total_ips ))
	echo "$next_index" > "$state_file"
}

elapsedCleanupTS=0

#Abbiamo:
#IN_MOVED_TO per i file .m3u8
#IN_MODIFY per i files .ts e .m3u8.tmp
#IN_DELETE per i file .ts
#IN_MOVED_FROM per i files .m3u8.tmp
#IN_CREATE per i files .m3u8.tmp e .ts
#we synchronize when .m3u8 is changed
if [ $eventName == "IN_MOVED_TO" ]
then
	#Per evitare che lo script venga eseguito piu volte per la stessa directory
	# Lock file specifico per la directory
	channelDirectoryMd5sum=$(echo "$channelDirectory" | md5sum | cut -d' ' -f1)
	LOCKFILE="/tmp/hls_sync_$channelDirectoryMd5sum.lock"
	# Lock usando file descriptor 200
	exec 200>"$LOCKFILE"
	#flock applica un lock esclusivo (non condiviso) al file aperto con FD 200
	#-n significa “non aspettare”: se il file è già lockato da un altro processo, esci subito
	#|| exit 1: se il lock non riesce, lo script esce con errore 1
	flock -n 200 || {
		echo "$(date)-$pid: Lock attivo per $channelDirectory, esco" >> $debugFileName
		exit 1
	}

	MAX_PARALLEL=5				# Max sincronizzazioni in parallelo
	BW_LIMIT=50000  			# Banda limite per ogni rsync (KB/s), opzionale
	#Poichè i files vengono trasferiti nei server definiti sotto in parallelo, 
	#il tempo totale necessario non è la somma dei tempi di ogni server ma è dato dal server
	#che impiega piu tempo, cioé quello in USA
	#Inoltre, per evitare di stressare gli stessi server, eseguiamo il sync ogni volta su server diversi
	serversToBeSynched="$(get_next_ip "mmsStorageIPList") $(get_next_ip "euExternalDeliveriesIPList") $(get_next_ip "usaExternalDeliveriesIPList")"
	#echo "serversToBeSynched: $serversToBeSynched" >> $debugFileName


	#Example of events using debug:
	#IN_CREATE --> 1258481.ts (/var/catramms/storage/MMSRepository/MMSLive/1/1258)
	#IN_MODIFY --> 1258481.ts (/var/catramms/storage/MMSRepository/MMSLive/1/1258)
	#IN_MODIFY --> 1258481.ts (/var/catramms/storage/MMSRepository/MMSLive/1/1258)
	#IN_MODIFY --> 1258481.ts (/var/catramms/storage/MMSRepository/MMSLive/1/1258)
	#temporary file: IN_CREATE --> 1258.m3u8.tmp
	#temporary file: IN_MODIFY --> 1258.m3u8.tmp
	#temporary file: IN_MOVED_FROM --> 1258.m3u8.tmp
	#IN_MOVED_TO --> 1258.m3u8 (/var/catramms/storage/MMSRepository/MMSLive/1/1258)


	#rsyncSource=$channelDirectory
	#rsyncDest=$(dirname $channelDirectory)

	#invece di sincronizzare una directory che cambia in continuazione, congeliamo la directory prima di sincronizzarla
	cp -r $channelDirectory $channelDirectory.$channelDirectoryMd5sum
	#in questo caso sincronizziamo i contenuti delle due directory e non le directory stesse, per cui serve / alla fine
	rsyncSource=$channelDirectory.$channelDirectoryMd5sum/
	rsyncDest=$channelDirectory/

	if [ $debug_rsync -eq 1 ]
	then
		#Warning: Permanently added '[116.202.53.105]:9255' (ED25519) to the list of known hosts.
		#Warning: Permanently added '[194.42.206.8]:9255' (ED25519) to the list of known hosts.
		export rsyncSource rsyncDest BW_LIMIT debugFileName pid
		parallel --env rsyncSource --env rsyncDest --env BW_LIMIT --env debugFileName --env pid --jobs "$MAX_PARALLEL" --bar --halt now,fail=1 \
			'{
					echo "$(date)-$pid ({}): Inizio sincronizzazione...$rsyncDest";
					# 1. Sincronizza solo i file .ts
					rsync -e "ssh -p 9255 -i ~/ssh-keys/hetzner-mms-key.pem -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no" \
						--partial --archive --progress --verbose --omit-dir-times --timeout=60 --inplace --bwlimit=$BW_LIMIT \
    				--include "*/" --include "*.ts" --exclude "*" \
    				"$rsyncSource" mms@{}:$rsyncDest
					status_ts=$?
					# 2. Sincronizza solo il file .m3u8
					rsync -e "ssh -p 9255 -i ~/ssh-keys/hetzner-mms-key.pem -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no" \
						--partial --archive --progress --verbose --omit-dir-times --timeout=60 --inplace --bwlimit=$BW_LIMIT \
					  --include "*/" --include "*.m3u8" --exclude "*" \
    				"$rsyncSource" mms@{}:$rsyncDest
					status_m3u8=$?
					if [[ $status_ts -eq 0 ]] && [[ $status_m3u8 -eq 0 ]]; then
						echo "$(date)-$pid ({}): COMPLETATO" >> $debugFileName
					else
						echo "$(date)-$pid ({}): ERRORE: rsync exited with status_ts $status_ts, status_m3u8 $status_m3u8" >> $debugFileName
					fi
				} >> $debugFileName 2>&1' \
				::: $serversToBeSynched
	else
		export rsyncSource rsyncDest BW_LIMIT debugFileName pid
		parallel --env rsyncSource --env rsyncDest --env BW_LIMIT --env debugFileName --env pid --jobs "$MAX_PARALLEL" --bar --halt now,fail=1 \
			'{
					# 1. Sincronizza solo i file .ts
					rsync -e "ssh -p 9255 -i ~/ssh-keys/hetzner-mms-key.pem -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no" \
						--partial --archive --omit-dir-times --timeout=60 --inplace --bwlimit=$BW_LIMIT \
    				--include "*/" --include "*.ts" --exclude "*" \
						$rsyncSource mms@{}:$rsyncDest
					status_ts=$?
					# 2. Sincronizza solo il file .m3u8
					rsync -e "ssh -p 9255 -i ~/ssh-keys/hetzner-mms-key.pem -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no" \
						--partial --archive --omit-dir-times --timeout=60 --inplace --bwlimit=$BW_LIMIT \
					  --include "*/" --include "*.m3u8" --exclude "*" \
						$rsyncSource mms@{}:$rsyncDest
					status_m3u8=$?
					if [[ $status_ts -ne 0 ]] || [[ $status_m3u8 -ne 0 ]]; then
						echo "$(date)-$pid ({}): ERRORE: rsync exited with status_ts $status_ts, status_m3u8 $status_m3u8" >> $debugFileName
					fi
				} >> $debugFileName 2>&1' \
				::: $serversToBeSynched
	fi
	rm -rf $channelDirectory.$channelDirectoryMd5sum

	if [ 0 -eq 1 ]; then 	#no clean
	#elimina i files .ts remoti dopo il retention retentionRemoteTSFilesInMinutes
	#Il comando non puo' durare piu di totalCommandTimeoutInSeconds
	retentionTSFileName=/home/mms/incrontab_retentionTS_$channelDirectoryMd5sum.time
	retentionIntrvalInSeconds=60
	now=$(date +%s)
	if [ ! -f "$retentionTSFileName" ]; then
    	echo "0" > "$retentionTSFileName"
	fi
	retentionTSLastRun=$(< "$retentionTSFileName")
	retentionTSDelta=$(( now - retentionTSLastRun ))
	if (( retentionTSDelta >= retentionIntrvalInSeconds )); then
		echo "$now" > "$retentionTSFileName"
		retentionRemoteTSFilesInMinutes=3
		totalCommandTimeoutInSeconds=5
		startCleanupTS=$(date +%s)
		for remote_host in $serversToBeSynched; do
			remote_dir=$(dirname "$channelDirectory")
			timeout ${totalCommandTimeoutInSeconds}s ssh -o ConnectTimeout=5 -o BatchMode=yes -o StrictHostKeyChecking=no \
				-p 9255 -i ~/ssh-keys/hetzner-mms-key.pem mms@"$remote_host" \
				"find '$remote_dir' -type f -name '*.ts' -mmin +$retentionRemoteTSFilesInMinutes -delete" >> $debugFileName 2>&1
			status=$?
			if [[ $status -ne 0 ]]; then
				echo "$(date)-$pid - Cleanup ts files FAILED on $remote_host (exit code $status)" >> "$debugFileName" 2>&1
			fi
		done
		endCleanupTS=$(date +%s)
		elapsedCleanupTS=$((endCleanupTS-startCleanupTS))
	fi
	fi
fi
end=$(date +%s)

elapsed=$((end-start))
echo "$(date)-$pid: @$eventName@ @$fileName@ @$channelDirectory@ @$elapsedCleanupTS@ @$elapsed@" >> $debugFileName

