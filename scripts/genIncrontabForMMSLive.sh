#!/bin/bash

#Questo script viene configurato in mms/conf/crontab.txt dei server 'origin' che devono aggiornare server 'mid-origin' tramite rete interna.
#Per questo motivo si applica solamente quando la variabile d'ambiente MMS_EXTERNAL_DELIVERY_SERVERS_TOBESYNCHED_BY_RSYNCD è inizializzata

debugFileName=/tmp/genIncrontabForMMSLive.log

source ~/mms/conf/mms-env.sh

if [ ! -f "$debugFileName" ]; then
	echo "" > $debugFileName
else
	filesize=$(stat -c %s $debugFileName)
	# 2 GB = 2 * 1024 * 1024 * 1024 = 2147483648 bytes
	if (( filesize > 2147483648 ))
	then
		cp -f $debugFileName $debugFileName.previous
		echo "" > $debugFileName
	fi
fi

if [[ -n "${MMS_EXTERNAL_DELIVERY_SERVERS_TOBESYNCHED_BY_RSYNCD:-}" ]]; then

	#questo server esegue un rsync verso i mid-origin tramite rete interna, quindi aggiorno se necessario le regole

	# File temporaneo per le nuove regole
	NEW_RULES=$(mktemp)

	# Genera nuove regole
	for dir in /var/mms/storage/MMSRepository/MMSLive/*/*; do
    		if [[ -d "$dir" && "$(basename "$dir")" =~ ^[0-9]+$ ]]; then
        		echo "$dir	IN_MODIFY,IN_CREATE,IN_DELETE,IN_MOVE_SELF,IN_MOVE	/opt/mms/MMS/scripts/incrontab.sh \$% \$@ \$#" >> "$NEW_RULES"
    		fi
	done

	# File temporaneo per le regole attuali
	CURRENT_RULES=$(mktemp)
	incrontab -l 2>/dev/null > "$CURRENT_RULES"

	# Confronto
	if ! cmp -s "$NEW_RULES" "$CURRENT_RULES"; then
    		echo "$(date) Regole incrontab cambiate: aggiorno..." >> $debugFileName
    		incrontab "$NEW_RULES"
	else
    		echo "$(date) Regole incrontab già aggiornate, nessuna modifica." >> $debugFileName
	fi

	# Pulizia
	#echo "$NEW_RULES $CURRENT_RULES" >> $debugFileName
	rm -f "$NEW_RULES" "$CURRENT_RULES"
else
    	echo "$(date) Regole regole non necessarie." >> $debugFileName
	#rimuove le regole eventualmente configurate su incrontab
	incrontab -r
fi

