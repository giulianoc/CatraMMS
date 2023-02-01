#!/bin/bash

WATCHFOLDER=/mnt/media-grid/ROSS/CLIPSTORE/WatchFolderRoot/CONTINUITA/MP_ARCHIVIO

watch() {

	#echo watching folder $1/ looking for files $2 minutes older. Tag name: $3.

	#while [[ true ]]
	#do
		find $WATCHFOLDER/$1 -type f -mmin +$2 -exec /home/mms/tools/promoMaterialIngest/promoMaterialIngest.sh {} $3 \;
	#done
}

watch AUDIO 1 PROMO_AUDIO
watch CLIP 1 PROMO_CLIP
watch IMAGE 1 PROMO_IMAGE

