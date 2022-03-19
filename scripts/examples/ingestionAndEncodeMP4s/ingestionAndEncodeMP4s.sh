
directory=/var/www/html/VOD

mmsBinaryHostname=mms-binary.mms-cloud-hub.com
mmsAPIHostname=mms-api.mms-cloud-hub.com
mmsUserKey=1
mmsAPIKey=1j1l1x1o1l1d1q1r1f1d1w1u1d1p1e1r1q1h1C1j1p1d1l1o111f1r1p1b1b1V1H1S1b1b14181719091914181613
ingestingBinaryThroughChunks=1

for pathname in $directory/*.mp4; do

	echo "pathname: $pathname"
	#i.e.: pathname: /path1/path2/path3/urban_male_lounge.mp4

	filenameWithExtension=$(basename -- "$pathname")
	#i.e.: filenameWithExtension: urban_male_lounge.mp4
	#echo "filenameWithExtension: $filenameWithExtension"

	extension="${filenameWithExtension##*.}"
	#i.e.: extension: mp4
	#echo "extension: $extension"

	filenameWithoutExtension="${filenameWithExtension%.*}"
	#i.e.: filenameWithoutExtension: urban_male_lounge
	#echo "filenameWithoutExtension: $filenameWithoutExtension"

	workflowJson=$filenameWithoutExtension.workflow.json
	sed "s/__TITLE__/$filenameWithoutExtension/g" ./ingestionAndEncodeMP4s.json | sed "s/__FILENAME__/$filenameWithExtension/g" | sed "s/__FILEFORMAT__/$extension/g" > "$workflowJson"

	ingestionOutputJson=$filenameWithoutExtension.ingestionOutput.json
	echo "ingesting json workflow..."
	curl -k -s -o "$ingestionOutputJson" -X POST -u $mmsUserKey:$mmsAPIKey -d "@$workflowJson" -H "Content-Type: application/json" https://$mmsAPIHostname/catramms/1.0.1/workflow

	addContentIngestionJobKey=$(jq -r --arg filenameWithExtension "$filenameWithExtension" '.tasks | .[] | select(.label==$filenameWithExtension) | .ingestionJobKey' "$ingestionOutputJson")
	#echo "addContentIngestionJobKey: $addContentIngestionJobKey"

	if [ $ingestingBinaryThroughChunks -eq 0 ]
	then
		echo "ingesting binary one step..."
		curl -k -s -X POST -u $mmsUserKey:$mmsAPIKey -T "$pathname" https://$mmsBinaryHostname/catramms/1.0.1/binary/$addContentIngestionJobKey
	else
		echo "ingesting binary through chunks..."
		./ingestionThroughChunks.sh $addContentIngestionJobKey "$pathname"
	fi

	echo ""
	echo ""
done

