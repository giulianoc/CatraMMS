
mmsUserKey=$1
mmsAPIKey=$2
title=$3
ingester=$4
retention=$5
binaryFilePathName=$6

#i.e. if IngestionNumber 2/171 was interrupted, continueFromIndex has to be 2
continueFromIndex=$7

if [ $# -lt 6 -o $# -gt 7 ]; then
	echo "Usage: $0 <mmsUserKey> <mmsAPIKey> <title> <ingester> <retention> <binaryFilePathName> [<continueFromIndex>]"

	exit 1
elif [ $# -eq 6 ]; then
	continueFromIndex=""
fi


if [ "$continueFromIndex" == "" ]; then

	#start from scratch

	filename=$(basename -- "$binaryFilePathName")
	extension="${filename##*.}"
	fileFormat=$extension

	#echo "./helper/ingestionWorkflow.sh $mmsUserKey \"$mmsAPIKey\" \"$title\" \"$ingester\" $retention $fileFormat"
	ingestionJobKey=$(./helper/ingestionWorkflow.sh $mmsUserKey "$mmsAPIKey" "$title" "$ingester" $retention $fileFormat)

	if [ "$ingestionJobKey" == "" ]; then
		echo "ingestionWorkflow.sh failed"
		cat ./helper/ingestionWorkflowResult.json
		rm ./helper/ingestionWorkflowResult.json
		echo ""

		exit 1
	fi

	rm ./helper/ingestionWorkflowResult.json

	echo "$ingestionJobKey" > /tmp/$filename.ingestionJobKey
else
	#it has to be continued, retrieve the ingestionJobKey

	#decrement needed by ingestionBinary.sh
	continueFromIndex=$((continueFromIndex-1))

	filename=$(basename -- "$binaryFilePathName")
	ingestionJobKey=$(cat /tmp/$filename.ingestionJobKey)
	if [ "$ingestionJobKey" == "" ]; then
		echo "ingestionJobKey not found, it is not possible to continue the upload"

		exit 1
	fi
fi


#echo "./helper/ingestionBinary.sh $mmsUserKey \"$mmsAPIKey\" $ingestionJobKey \"$binaryFilePathName\""
./helper/ingestionBinary.sh $mmsUserKey "$mmsAPIKey" $ingestionJobKey "$binaryFilePathName" $continueFromIndex

rm /tmp/$filename.ingestionJobKey

