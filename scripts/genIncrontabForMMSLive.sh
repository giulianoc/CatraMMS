#!/bin/bash

debugFileName=/tmp/genIncrontabForMMSLive.log

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

# File temporaneo per le nuove regole
NEW_RULES=$(mktemp)

# Genera nuove regole
for dir in /var/catramms/storage/MMSRepository/MMSLive/*/*; do
    if [[ -d "$dir" && "$(basename "$dir")" =~ ^[0-9]+$ ]]; then
        echo "$dir	IN_MODIFY,IN_CREATE,IN_DELETE,IN_MOVE_SELF,IN_MOVE	/opt/catramms/CatraMMS/scripts/incrontab.sh \$% \$@ \$#" >> "$NEW_RULES"
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
