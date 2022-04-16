
if [ $# -ne 2 ]; then
	echo "Usage: $0 <mmsUserKey> <mmsAPIKey>"

	exit 1
fi

mmsUserKey=$1
mmsAPIKey=$2

curl -k -v -X POST -u $mmsUserKey:$mmsAPIKey -d @./ingestionWorkflow.json -H "Content-Type: application/json" https://mms-api.cibortv-mms.com/catramms/1.0.1/workflow

