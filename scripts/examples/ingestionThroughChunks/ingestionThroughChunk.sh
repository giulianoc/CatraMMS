
mmsUserKey=$1
mmsAPIKey=$2
title=$3
tag=$4
ingester=$5
retention=$6
binaryFilePathName=$7
#i.e. if IngestionNumber 2/171 was interrupted, continueFromIndex has to be 2
continueFromIndex=$8

if [ $# -lt 7 -o $# -gt 8 ]; then
	echo "Usage: $0 <mmsUserKey> <mmsAPIKey> <title> <tag> <ingester> <retention> <binaryFilePathName> [<continueFromIndex>]"

	echo "The current parameters number is: $#, it shall be 7 or 8"
	paramIndex=1
	for param in "$@"
	do
		echo "Param #$paramIndex: $param";
		paramIndex=$((paramIndex + 1));
	done

	exit 1
elif [ $# -eq 7 ]; then
	continueFromIndex=""
fi


if [ "$continueFromIndex" = "" ]; then

	#start from scratch

	filename=$(basename -- "$binaryFilePathName")
	extension="${filename##*.}"
	fileFormat=$extension

	#echo "./helper/ingestionWorkflow.sh $mmsUserKey \"$mmsAPIKey\" \"$title\" \"$tag\" \"$ingester\" $retention $fileFormat"
	ingestionJobKey=$(./helper/ingestionWorkflow.sh $mmsUserKey "$mmsAPIKey" "$title" "$tag" "$ingester" $retention $fileFormat)

	if [ "$ingestionJobKey" = "" ]; then
		echo "ingestionWorkflow.sh failed"
		cat ./helper/ingestionWorkflowResult.json
		rm -f ./helper/ingestionWorkflowResult.json
		echo ""

		exit 1
	fi

	rm -f ./helper/ingestionWorkflowResult.json

	echo "$ingestionJobKey" > /tmp/$filename.ingestionJobKey
else
	#it has to be continued, retrieve the ingestionJobKey

	#decrement needed by ingestionBinary.sh
	continueFromIndex=$((continueFromIndex-1))

	filename=$(basename -- "$binaryFilePathName")
	ingestionJobKey=$(cat /tmp/$filename.ingestionJobKey)
	if [ "$ingestionJobKey" = "" ]; then
		echo "ingestionJobKey not found, it is not possible to continue the upload"

		exit 1
	fi
fi


#echo "./helper/ingestionBinary.sh $mmsUserKey \"$mmsAPIKey\" $ingestionJobKey \"$binaryFilePathName\""
./helper/ingestionBinary.sh $mmsUserKey "$mmsAPIKey" $ingestionJobKey "$binaryFilePathName" $continueFromIndex

if [ $? -ne 0 ]; then
	exit 1
fi

rm -f /tmp/$filename.ingestionJobKey

