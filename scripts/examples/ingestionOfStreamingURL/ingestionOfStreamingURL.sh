#!/bin/bash

if [ $# -lt 8 ]; then
	echo "Usage: $0 <mmsUserKey> <mmsAPIKey> <tag> <ingester> <retention> <encodersPool> <encodingProfilesSet> <streamingURLFile>"

	echo "The current parameters number is: $#, it shall be 9"
	paramIndex=1
	for param in "$@"
	do
		echo "Param #$paramIndex: $param";
		paramIndex=$((paramIndex + 1));
	done

	exit 1
fi

mmsUserKey=$1
mmsAPIKey=$2
tag=$3
ingester=$4
retention=$5
encodersPool=$6
encodingProfilesSet=$7
streamingURLFile=$8

mmsAPIHostName=mms-api.catramms-cloud.com


while read titleAndtreamingURL; do
	if [ "$titleAndtreamingURL" = "" ]; then
		continue
	fi

	#echo "titleAndtreamingURL: $titleAndtreamingURL"

	title=$(echo $titleAndtreamingURL | cut -d ";" -f 1)
	streamingURL=$(echo $titleAndtreamingURL | cut -d ";" -f 2)
	#echo "title: $title"
	#echo "streamingURL: $streamingURL"

	encodedStreamingURL=${streamingURL//\//"\\/"}
	encodedStreamingURL=${encodedStreamingURL//\&/"\\&"}
	#echo "encodedStreamingURL: $encodedStreamingURL"

	sed "s/\${title}/$title/g" ./helper/ingestionWorkflow.json | sed "s/\${streamingURL}/$encodedStreamingURL/g" | sed "s/\${tag}/$tag/g" | sed "s/\${ingester}/$ingester/g" | sed "s/\${retention}/$retention/g" | sed "s/\${encodersPool}/$encodersPool/g" | sed "s/\${encodingProfilesSet}/$encodingProfilesSet/g" > ./helper/ingestionWorkflow.json.new
	curl -o ./helper/ingestionWorkflowResult.json -k -s -X POST -u $mmsUserKey:$mmsAPIKey -d @./helper/ingestionWorkflow.json.new -H "Content-Type: application/json" https://$mmsAPIHostName/catramms/1.0.1/workflow
done < "$streamingURLFile"


