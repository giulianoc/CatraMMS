
if [ $# -ne 8 ]; then
	echo "Usage: $0 <mmsUserKey> <mmsAPIKey> <title> <tag> <ingester> <profileset> <retention> <fileFormat> ($#)"

	exit 1
fi

#mmsAPIHostName=mms-api.catramms-cloud.com
mmsAPIHostName=mms-api.cibortv-mms.com

mmsUserKey=$1
mmsAPIKey=$2
title=$3
tag=$4
ingester=$5
profileSet=$6
retention=$7
fileFormat=$8

#if ! command -v jq &> /dev/null
if ! [ -x "$(command -v jq)" ]
then
    echo "The 'jq' command could not be found, please install it"

    exit
fi

sed "s/\${title}/$title/g" ./helper/ingestionWorkflow.json | sed "s/\${tag}/$tag/g" | sed "s/\${ingester}/$ingester/g" | sed "s/\${profileSet}/$profileSet/g" | sed "s/\${retention}/$retention/g" | sed "s/\${fileFormat}/$fileFormat/g" > ./helper/ingestionWorkflow.json.new

responseCode=$(curl -o ./helper/ingestionWorkflowResult.json -w "%{response_code}" -k -s -X POST -u $mmsUserKey:$mmsAPIKey -d @./helper/ingestionWorkflow.json.new -H "Content-Type: application/json" https://$mmsAPIHostName/catramms/1.0.1/workflow)
if [ "$responseCode" -ne "201" ]; then
	echo "$(date +%Y-%m-%d-%H:%M:%S): FAILURE, Ingestion response code: $responseCode"

	exit 2
fi

rm ./helper/ingestionWorkflow.json.new

#print ingestionJobKey
jq '.tasks[] | select(.type == "Add-Content") | .ingestionJobKey' ./helper/ingestionWorkflowResult.json

#2022-06-29: not removed because printed by the caller script in case of error
#	The file is also removed by the caller script
#rm ./helper/ingestionWorkflowResult.json


