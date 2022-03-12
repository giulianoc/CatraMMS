
BEGIN {
	FS="\t";
	outputPathName="./output.sh"

	printf("#!/bin/bash\n\n") > outputPathName;
}

{
	title=$1
	serverName=$2
	serverPort=$3
	encodersPool=$4
	serverURI=$5

	printf("\n") >> outputPathName;

	gsub(/\//, "\\\/", title);
	gsub(/\"/, "\\\\\\\\\\\\\\\\\\\"", title);

	gsub(/\//, "\\\/", serverURI);
	gsub(/\"/, "\\\\\\\\\\\\\\\\\\\"", serverURI);

	printf("sed \"s/__title__/%s/g\" ./utility/seriec_addIPChannelTemplate.json | sed \"s/__serverName__/%s/g\" | sed \"s/__serverPort__/%s/g\" | sed \"s/__encodersPool__/%s/g\" | sed \"s/__serverURI__/%s/g\" > ./outputAddIPChannel.json\n", title, serverName, serverPort, encodersPool, serverURI) >> outputPathName;

	printf("curl -k -u %s:%s -d @./outputAddIPChannel.json -H \"Content-Type: application/json\" https://%s/catramms/1.0.1/conf/channel\n", userKey, apiKey, mmsApiHostname) >> outputPathName;
}

