
directory=/opt/catrasoftware/CatraMMS/scripts/examples/ingestionAndEncodeMP4s

mmsUserKey=1
mmsAPIKey=1j1l1x1o1l1d1q1r1f1d1w1u1d1p1e1r1q1h1C1j1p1d1l1o111f1r1p1b1b1V1H1S1b1b14181719091914181613

for pathname in $directory/*.mp4; do

	echo "pathname: $pathname"
	#i.e.: pathname: /path1/path2/path3/urban_male_lounge.mp4

	filenameWithExtension=$(basename -- "$pathname")
	#i.e.: filenameWithExtension: urban_male_lounge.mp4
	echo "filenameWithExtension: $filenameWithExtension"

	extension="${filenameWithExtension##*.}"
	#i.e.: extension: mp4
	echo "extension: $extension"

	filenameWithoutExtension="${filenameWithExtension%.*}"
	#i.e.: filenameWithoutExtension: urban_male_lounge
	echo "filenameWithoutExtension: $filenameWithoutExtension"

	workflowJson=$filenameWithoutExtension.workflow.json
	sed "s/__TITLE__/$filenameWithoutExtension/g" ./ingestionAndEncodeMP4s.json | sed "s/__FILENAME__/$filenameWithExtension/g" | sed "s/__FILEFORMAT__/$extension/g" > $workflowJson

	ingestionOutputJson=$filenameWithoutExtension.ingestionOutput.json
	#curl -k -s -o $ingestionOutputJson -X POST -u $mmsUserKey:$mmsAPIKey -d @$workflowJson -H "Content-Type: application/json" https://mms-api.mms-cloud-hub.com/catramms/1.0.1/workflow

	addContentIngestionJobKey=$(jq -r --arg filenameWithExtension "$filenameWithExtension" '.tasks | .[] | select(.label==$filenameWithExtension) | .ingestionJobKey' $ingestionOutputJson)
	echo "addContentIngestionJobKey: $addContentIngestionJobKey"

	curl -k -s -X POST -u $mmsUserKey:$mmsAPIKey -T $pathname https://mms-binary.mms-cloud-hub.com/catramms/1.0.1/binary/$addContentIngestionJobKey
done

