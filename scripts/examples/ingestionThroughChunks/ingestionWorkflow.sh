
if [ $# -lt 4 -o $# -gt 6 ]; then
	echo "Usage: $0 <mmsUserKey> <mmsAPIKey> <title> <ingester> [<retention> <fileFormat>]"

	exit 1
fi

mmsAPIHostName=mms-api.cibortv-mms.com

mmsUserKey=$1
mmsAPIKey=$2
title=$3
ingester=$4
if [ $# -gt 4 ]; then
	retention=$5
else
	retention="3d"
fi
if [ $# -gt 5 ]; then
	fileFormat=$6
else
	fileFormat="mp4"
fi

sed "s/\${title}/$title/g" ./ingestionWorkflow.json | sed "s/\${ingester}/$ingester/g" | sed "s/\${retention}/$retention/g" | sed "s/\${fileFormat}/$fileFormat/g" > ./ingestionWorkflow.json.new

#curl -k -v -X POST -u $mmsUserKey:$mmsAPIKey -d @./ingestionWorkflow.json.new -H "Content-Type: application/json" https://$mmsAPIHostName/catramms/1.0.1/workflow

#rm ./ingestionWorkflow.json.new

