#!/bin/bash

debug=1
debug_rsync=0
debug_event=0
#2021-01-08: it has to be a path in home user otherwise incron does not run the script
debugFileName=/home/mms/incrontab.log

source ~/mms/conf/mms-env.sh

start=$(date +%s%3N)
pid=$$

if [ ! -f "$debugFileName" ]; then
	echo "" > $debugFileName
else
	filesize=$(stat -c %s $debugFileName)
	# 1 GB = 1 * 1024 * 1024 * 1024 = 1073741824 bytes
	if (( filesize > 1073741824 ))
	then
		echo "" > $debugFileName
	fi              
fi

if [ $# -ne 3 ]
then
	if [ $debug -eq 1 ]
	then
		echo "$(date +'%Y-%m-%d %H:%M:%S')-$pid: usage $0 <eventName> <channelDirectory> <fileName>" >> $debugFileName
	fi

	exit
fi

eventName=$1
channelDirectory=$2
fileName=$3

elapsedCopyInMilliSecs=0

#Caso (1). Nel caso di un encoder che genera HLS, abbiamo:
#IN_MOVED_TO per i file .m3u8
#IN_MODIFY per i files .ts e .m3u8.tmp
#IN_DELETE per i file .ts
#IN_MOVED_FROM per i files .m3u8.tmp
#IN_CREATE per i files .m3u8.tmp e .ts
#we synchronize when .m3u8 is changed (IN_MOVED_TO)
#Caso (2). Nel caso invece in cui siamo nello scenario in cui un externalDelivery riceve i files dall'encoder (tramite rsync) e deve replicarli
#su un altro storage, in questo caso è sufficiente che il file .m3u8 sia cambiato
if [[ "$eventName" == "IN_MOVED_TO" || "$fileName" == *.m3u8 ]]
then
	#Per evitare che lo script venga eseguito piu volte per la stessa directory
	# Lock file specifico per la directory
	channelDirectoryMd5sum=$(echo "$channelDirectory" | md5sum | cut -d' ' -f1)
	LOCKFILE="/tmp/hls_sync_$channelDirectoryMd5sum.lock"
	# Apre il file "$LOCKFILE" in scrittura e gli associa il file descriptor 200
	exec 200>"$LOCKFILE"
	#flock applica un lock esclusivo (non condiviso) al file aperto con FD 200
	#-n significa “non aspettare”: se il file è già lockato da un altro processo, esci subito
	#|| exit 1: se il lock non riesce, lo script esce con errore 1
	#Il lock viene rilasciato automaticamente
	# - quando lo script termina, perché la shell chiude tutti i file descriptor aperti, incluso il 200.
	# - oppure quando chiudi esplicitamente quel fd con exec 200>&-
	#La string "Lock attivo ($fileName)" viene usato in servicesStatusLibrary.sh per il suo monitoring
	flock -n 200 || {
		end=$(date +%s%3N)
		totalElapsed=$((end-start))
		echo "@$(date +'%Y-%m-%d %H:%M:%S')-$pid@ @ERROR@ @$eventName@ @$channelDirectory@ @$fileName@ @$elapsedCopyInMilliSecs@ @$totalElapsed@ Lock attivo ($LOCKFILE), skip" >> $debugFileName

		exit 1
	}

	MAX_PARALLEL=5				# Max sincronizzazioni in parallelo
	BW_LIMIT=50000  			# Banda limite per ogni rsync (KB/s), opzionale
	#Poichè i files vengono trasferiti nei server definiti sotto in parallelo, 
	#il tempo totale necessario non è la somma dei tempi di ogni server ma è dato dal server
	#che impiega piu tempo, cioé quello in USA
	#Inoltre, per evitare di stressare gli stessi server, eseguiamo il sync ogni volta su server diversi
	#serversToBeSynched="$(get_next_ip "mmsStorageIPList") $(get_next_ip "euExternalDeliveriesIPList") $(get_next_ip "usaExternalDeliveriesIPList") $(get_next_ip "usa2ExternalDeliveriesIPList")"
	serversToBeSynched="$MMS_RSYNC_EXTERNAL_DELIVERY_SERVERS" # defined in mms-env.sh
	#echo "serversToBeSynched: $serversToBeSynched" >> $debugFileName

	#se non esiste il file ~/.ssh/known_hosts, lo creo e lo pre-popolo per evitare messaggi tipo
	#Warning: Permanently added '[91.222.174.119]:9255' (ED25519) to the list of known hosts.
	KNOWN="$HOME/.ssh/known_hosts"
	if [[ ! -f "$KNOWN" ]]; then
		mkdir -p "$HOME/.ssh"; chmod 700 "$HOME/.ssh"
		: > "KNOWN"; chmod 600 "$KNOWN"

		for H in $serversToBeSynched; do
  		k=$(ssh-keyscan -p 9255 -T 5 "$H" 2>/dev/null) || continue
  		# riscrive l’host iniziale in formato "[host]:9255 ..."
  		printf '[%s]:9255 %s\n' "$H" "${k#* }" >> "$KNOWN"
		done
		# verifica:
		# ssh-keygen -F "[91.222.174.119]:9255"
	fi

	identityFile=~/ssh-keys/hetzner-mms-key.pem
	if [[ ! -f "$identityFile" ]]; then
		end=$(date +%s%3N)
		totalElapsed=$((end-start))
		echo "@$(date +'%Y-%m-%d %H:%M:%S')-$pid@ @ERROR@ @$eventName@ @$channelDirectory@ @$fileName@ @$elapsedCopyInMilliSecs@ @$totalElapsed@ identity files does not exist ($identityFile)" >> $debugFileName

		exit 1
	fi

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
	startCopy=$(date +%s%3N)
	#cp -r $channelDirectory $channelDirectory.$channelDirectoryMd5sum
	#rsync -a per preservare timestamp, permessi, ownership, ... per evitare che i file siano “modificati” e li risincronizza anche se il contenuto è identico
	#--ignore-missing-args perchè durante la sincronizzazione alcuni file presenti all’inizio nel sorgente sono scomparsi (cancellati o sovrascritti) prima che rsync riuscisse a copiarli
	# 	Questo è normale nello streaming live
	rsync -a --ignore-missing-args $channelDirectory/ $channelDirectory.$channelDirectoryMd5sum/
	status_copy=$?
	endCopy=$(date +%s%3N)
	if [[ $status_copy -ne 0 ]]; then
		end=$(date +%s%3N)
		totalElapsed=$((end-start))
		echo "@$(date +'%Y-%m-%d %H:%M:%S')-$pid@ @ERROR@ @$eventName@ @$channelDirectory@ @$fileName@ @$elapsedCopyInMilliSecs@ @$totalElapsed@ copy failed ($fileName) per $channelDirectory -> $channelDirectory.$channelDirectoryMd5sum" >> $debugFileName

		exit 1
	fi
  elapsedCopyInMilliSecs=$((endCopy-startCopy))
	#in questo caso sincronizziamo i contenuti delle due directory e non le directory stesse, per cui serve / alla fine
	rsyncSource=$channelDirectory.$channelDirectoryMd5sum/
	rsyncDest=$channelDirectory/

	#e' impoetante questo formato perchè viene usato da servicesStatusLibrary.sh (mms_incrontab_check_rsync)
	sDate=$(date +'%Y-%m-%d %H:%M:%S')
	if [ $debug_rsync -eq 1 ]
	then
		#Warning: Permanently added '[116.202.53.105]:9255' (ED25519) to the list of known hosts.
		#Warning: Permanently added '[194.42.206.8]:9255' (ED25519) to the list of known hosts.
		#Le variabili esportate vengono ereditate dai processi figli dello script (parallel, rsync)
		export rsyncSource rsyncDest BW_LIMIT debugFileName pid fileName sDate identityFile
		parallel --env rsyncSource --env rsyncDest --env BW_LIMIT --env debugFileName --env pid --env fileName --env sDate --env identityFile --jobs "$MAX_PARALLEL" --bar --halt now,fail=1 \
			'{
					SSH_OPTS="-p 9255 -i $identityFile -o UserKnownHostsFile=~/.ssh/known_hosts -o StrictHostKeyChecking=yes"
					echo "@$(date)-$pid ({})@: @$fileName@ @INFO@ Inizio sincronizzazione...$rsyncDest" >> $debugFileName
					# 1. Sincronizza solo i file .ts
					timeout 15s rsync -e "ssh $SSH_OPTS" \
						--partial --archive --progress --verbose --omit-dir-times --timeout=15 --inplace --bwlimit=$BW_LIMIT \
    				--include "*/" --include "*.ts" --exclude "*" \
    				"$rsyncSource" mms@{}:$rsyncDest
					status_ts=$?
					if [[ $status_ts -eq 0 ]]; then
						# 2. Sincronizza solo il file .m3u8
						timeout 15s rsync -e "ssh $SSH_OPTS" \
							--partial --archive --progress --verbose --omit-dir-times --timeout=15 --inplace --bwlimit=$BW_LIMIT \
					  	--include "*/" --include "*.m3u8" --exclude "*" \
    					"$rsyncSource" mms@{}:$rsyncDest
						status_m3u8=$?
						if [[ $status_m3u8 -ne 0 ]]; then
							#La string "RSYNC ERROR ($fileName)" viene usato in servicesStatusLibrary.sh per il suo monitoring
							echo "@$sDate-$pid ({})@: @ERROR@ @$fileName@ rsync failed (m3u8) exited with status_m3u8: @$status_m3u8@ (124: timeout)" >> $debugFileName
						fi
					else
						echo "@$sDate-$pid ({})@: @ERROR@ @$fileName@ rsync failed (ts) exited with status_ts: @$status_ts@ (124: timeout)" >> $debugFileName
					fi
				} >> $debugFileName 2>&1' \
				::: $serversToBeSynched
	else
		#Le variabili esportate vengono ereditate dai processi figli dello script (parallel, rsync)
		export rsyncSource rsyncDest BW_LIMIT debugFileName pid fileName sDate identityFile
		parallel --env rsyncSource --env rsyncDest --env BW_LIMIT --env debugFileName --env pid --env fileName --env sDate --env identityFile --jobs "$MAX_PARALLEL" --bar --halt now,fail=1 \
			'{
					SSH_OPTS="-p 9255 -i $identityFile -o UserKnownHostsFile=~/.ssh/known_hosts -o StrictHostKeyChecking=yes"
					# 1. Sincronizza solo i file .ts
					timeout 15s rsync -e "ssh $SSH_OPTS" \
						--partial --archive --omit-dir-times --timeout=15 --inplace --bwlimit=$BW_LIMIT \
    				--include "*/" --include "*.ts" --exclude "*" \
						$rsyncSource mms@{}:$rsyncDest
					status_ts=$?
					if [[ $status_ts -eq 0 ]]; then
						# 2. Sincronizza solo il file .m3u8
						timeout 15s rsync -e "ssh $SSH_OPTS" \
							--partial --archive --omit-dir-times --timeout=15 --inplace --bwlimit=$BW_LIMIT \
					  	--include "*/" --include "*.m3u8" --exclude "*" \
							$rsyncSource mms@{}:$rsyncDest
						status_m3u8=$?
						if [[ $status_m3u8 -ne 0 ]]; then
							#La string "RSYNC ERROR ($fileName)" viene usato in servicesStatusLibrary.sh per il suo monitoring
							echo "@$sDate-$pid ({})@: @ERROR@ @$fileName@ rsync failed (m3u8) exited with status_m3u8: @$status_m3u8@ (124: timeout)" >> $debugFileName
						fi
					else
						echo "@$sDate-$pid ({})@: @ERROR@ @$fileName@ rsync failed (ts) exited with status_ts: @$status_ts@ (124: timeout)" >> $debugFileName
					fi
				} >> $debugFileName 2>&1' \
				::: $serversToBeSynched
	fi
	rm -rf $channelDirectory.$channelDirectoryMd5sum

	end=$(date +%s%3N)
	totalElapsed=$((end-start))
	echo "@$(date +'%Y-%m-%d %H:%M:%S')-$pid@ @INFO@ @$eventName@ @$channelDirectory@ @$fileName@ @$elapsedCopyInMilliSecs@ @$totalElapsed@" >> $debugFileName
else
	if [ $debug_event -eq 1 ]
	then
		end=$(date +%s%3N)
		totalElapsed=$((end-start))
		echo "@$(date +'%Y-%m-%d %H:%M:%S')-$pid@ @INFO@ @$eventName@ @$channelDirectory@ @$fileName@ @$elapsedCopyInMilliSecs@ @$totalElapsed@" >> $debugFileName
	fi
fi

