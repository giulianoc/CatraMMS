
if [ $# -ne 6 ]; then
	echo "Usage: $0 <mmsUserKey> <mmsAPIKey> <title> <ingester> <retention> <binaryFilePathName>]"

	exit 1
fi

mmsUserKey=$1
mmsAPIKey=$2
title=$3
ingester=$4
retention=$5
binaryFilePathName=$6


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

#echo "./helper/ingestionBinary.sh $mmsUserKey \"$mmsAPIKey\" $ingestionJobKey \"$binaryFilePathName\""
./helper/ingestionBinary.sh $mmsUserKey "$mmsAPIKey" $ingestionJobKey "$binaryFilePathName"

