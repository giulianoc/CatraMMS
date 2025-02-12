#!/bin/bash

mmsUserKey=$1
mmsAPIKey=$2
title=$3
tag=$4
uniqueName=$5
ingester=$6
retention=$7
binaryFilePathName=$8
#i.e. if IngestionNumber 2/171 was interrupted, continueFromIndex has to be 2
continueFromIndex=$9

scriptPathName=$(readlink -f "$0")
scriptDirectory=$(dirname "$scriptPathName")

if [ $# -lt 8 -o $# -gt 9 ]; then
	echo "Usage: $0 <mmsUserKey> <mmsAPIKey> <title> <tags comma separated. i.e.: \"\\\"tag 1\\\", \\\"Tag 2\\\"\"> <uniqueName> <ingester> <retention> <binaryFilePathName> [<continueFromIndex>]"

	echo "The current parameters number is: $#, it shall be 8 or 9"
	paramIndex=1
	for param in "$@"
	do
		echo "Param #$paramIndex: $param";
		paramIndex=$((paramIndex + 1));
	done

	exit 1
elif [ $# -eq 8 ]; then
	continueFromIndex=""
fi


if [ "$continueFromIndex" = "" ]; then

	#start from scratch

	filename=$(basename -- "$binaryFilePathName")
	extension="${filename##*.}"
	fileFormat=$extension

	#echo "./helper/ingestionWorkflow.sh $mmsUserKey \"$mmsAPIKey\" \"$title\" \"$tag\" \"$ingester\" $retention $fileFormat"
	ingestionJobKey=$($scriptDirectory/helper/ingestionWorkflow.sh $mmsUserKey "$mmsAPIKey" "$title" "$tag" "$uniqueName" "$ingester" $retention $fileFormat)

	if [ "$ingestionJobKey" = "" ]; then
		echo "ingestionWorkflow.sh failed"
		cat $scriptDirectory/helper/ingestionWorkflowResult.json
		rm -f $scriptDirectory/helper/ingestionWorkflowResult.json
		echo ""

		exit 1
	fi

	rm -f $scriptDirectory/helper/ingestionWorkflowResult.json

	echo "$ingestionJobKey" > "/tmp/$filename.ingestionJobKey"
else
	#it has to be continued, retrieve the ingestionJobKey

	#decrement needed by ingestionBinary.sh
	continueFromIndex=$((continueFromIndex-1))

	filename=$(basename -- "$binaryFilePathName")
	ingestionJobKey=$(cat "/tmp/$filename.ingestionJobKey")
	if [ "$ingestionJobKey" = "" ]; then
		echo "ingestionJobKey not found, it is not possible to continue the upload"

		exit 1
	fi
fi


#echo "$scriptDirectory/helper/ingestionBinary.sh $mmsUserKey \"$mmsAPIKey\" $ingestionJobKey \"$binaryFilePathName\""
$scriptDirectory/helper/ingestionBinary.sh $mmsUserKey "$mmsAPIKey" $ingestionJobKey "$binaryFilePathName" $continueFromIndex

if [ $? -ne 0 ]; then
	exit 1
fi

rm -f "/tmp/$filename.ingestionJobKey"

