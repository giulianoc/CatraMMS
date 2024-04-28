
mmsUserKey=$1
mmsAPIKey=$2
title=$3
tag=$4
ingester=$5
profileSet=$6
retention=$7
binaryFilePathName=$8

if [ $# -ne 8 ]; then
	echo "Usage: $0 <mmsUserKey> <mmsAPIKey> <title> <tag> <ingester> <profileset> <retention> <binaryFilePathName>"

	echo "The current parameters number is: $#, it shall be 8"
	paramIndex=1
	for param in "$@"
	do
		echo "Param #$paramIndex: $param";
		paramIndex=$((paramIndex + 1));
	done

	exit 1
fi

filename=$(basename -- "$binaryFilePathName")
extension="${filename##*.}"
fileFormat=$extension

#echo "./helper/ingestionWorkflow.sh $mmsUserKey \"$mmsAPIKey\" \"$title\" \"$tag\" \"$ingester\" $profileSet $retention $fileFormat"
ingestionJobKey=$(./helper/ingestionWorkflow.sh $mmsUserKey "$mmsAPIKey" "$title" "$tag" "$ingester" $profileSet $retention $fileFormat)
#echo "ingestionJobKey: $ingestionJobKey"

if [ "$ingestionJobKey" = "" ]; then
	echo "ingestionWorkflow.sh failed"
	cat ./helper/ingestionWorkflowResult.json
	rm -f ./helper/ingestionWorkflowResult.json
	echo ""

	exit 1
fi

rm -f ./helper/ingestionWorkflowResult.json

echo "$ingestionJobKey" > /tmp/$filename.ingestionJobKey


echo "./helper/ingestionBinary.sh $mmsUserKey \"$mmsAPIKey\" $ingestionJobKey \"$binaryFilePathName\""
./helper/ingestionBinary.sh $mmsUserKey "$mmsAPIKey" $ingestionJobKey "$binaryFilePathName"

if [ $? -ne 0 ]; then
	exit 1
fi

rm -f /tmp/$filename.ingestionJobKey

