
if [ $# -lt 5 -o $# -gt 7 ]; then
	echo "Usage: $0 <mmsUserKey> <mmsAPIKey> <title> <tag> <ingester> [<retention> <fileFormat>]"

	exit 1
fi

mmsAPIHostName=mms-api.catramms-cloud.com

mmsUserKey=$1
mmsAPIKey=$2
title=$3
tag=$4
ingester=$5
if [ $# -gt 5 ]; then
	retention=$6
else
	retention="3d"
fi
if [ $# -gt 6 ]; then
	fileFormat=$7
else
	fileFormat="mp4"
fi

#if ! command -v jq &> /dev/null
if ! [ -x "$(command -v jq)" ]
then
    echo "The 'jq' command could not be found, please install it"

    exit
fi

sed "s/\${title}/$title/g" ./helper/ingestionWorkflow.json | sed "s/\${tag}/$tag/g" | sed "s/\${ingester}/$ingester/g" | sed "s/\${retention}/$retention/g" | sed "s/\${fileFormat}/$fileFormat/g" > ./helper/ingestionWorkflow.json.new

curl -o ./helper/ingestionWorkflowResult.json -k -s -X POST -u $mmsUserKey:$mmsAPIKey -d @./helper/ingestionWorkflow.json.new -H "Content-Type: application/json" https://$mmsAPIHostName/catramms/1.0.1/workflow

rm ./helper/ingestionWorkflow.json.new

#print ingestionJobKey
jq '.tasks[] | select(.type == "Add-Content") | .ingestionJobKey' ./helper/ingestionWorkflowResult.json

#2022-06-29: not removed because printed by the caller script in case of error
#	The file is also removed by the caller script
#rm ./helper/ingestionWorkflowResult.json


