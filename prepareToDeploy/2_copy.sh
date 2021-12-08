#!/bin/bash

date

version=$(cat ./version.txt)

currentDir=$(pwd)
moduleName=$(basename $currentDir)

linuxName=$(cat /etc/os-release | grep "^ID=" | cut -d'=' -f2)
#linuxName using centos will be "centos", next remove "
linuxName=$(echo $linuxName | awk '{ if (substr($0, 0, 1) == "\"") printf("%s", substr($0, 2, length($0) - 2)); else printf("%s", $0) }')

tarFileName=$moduleName-$version-$linuxName.tar.gz

#make it downloadable from public
echo -n "Load package to MMSRepository-free? " 
read deploy
if [ "$deploy" == "y" ]; then
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@cibortv-mms-api-gui-1:/var/catramms/storage/MMSRepository-free/packages
fi

echo -n "deploy su mms cloud/test? " 
read deploy
if [ "$deploy" == "y" ]; then
	echo "mms-t-api-gui-1"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@mms-t-api-gui-1:/opt/catramms
	date

	echo "mms-t-engine-db-1"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@mms-t-engine-db-1:/opt/catramms
	date

	echo "mms-t-transcoder-fr-1"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@mms-t-transcoder-fr-1:/opt/catramms
	date
fi

echo -n "deploy su cibor? " 
read deploy
if [ "$deploy" == "y" ]; then
	echo "cibortv-mms-api-gui-1"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@cibortv-mms-api-gui-1:/opt/catramms
	date

	echo "cibortv-mms-api-gui-2"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@cibortv-mms-api-gui-2:/opt/catramms
	date

	echo "cibortv-mms-engine-db-1"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@cibortv-mms-engine-db-1:/opt/catramms
	date

	echo "cibortv-mms-engine-db-2"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@cibortv-mms-engine-db-2:/opt/catramms
	date

	echo "cibortv-mms-engine-db-3"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@cibortv-mms-engine-db-3:/opt/catramms
	date

	echo "cibortv-mms-transcoder-fr-1"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@cibortv-mms-transcoder-fr-1:/opt/catramms
	date

	echo "cibortv-mms-transcoder-fr-2"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@cibortv-mms-transcoder-fr-2:/opt/catramms
	date

	echo "cibortv-mms-transcoder-fr-3"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@cibortv-mms-transcoder-fr-3:/opt/catramms
	date

	echo "cibortv-mms-transcoder-fr-4"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@cibortv-mms-transcoder-fr-4:/opt/catramms
	date

	echo "cibortv-mms-transcoder-fr-11"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@cibortv-mms-transcoder-fr-11:/opt/catramms
	date

	echo "cibortv-mms-transcoder-fr-12"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@cibortv-mms-transcoder-fr-12:/opt/catramms
	date

	echo "cibortv-mms-transcoder-ita-1"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@cibortv-mms-transcoder-ita-1:/opt/catramms
	date

	echo "cibortv-mms-transcoder-ita-2"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@cibortv-mms-transcoder-ita-2:/opt/catramms
	date

	echo "cibortv-mms-transcoder-itaita-1"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@cibortv-mms-transcoder-itaita-1:/opt/catramms
	date

	echo "cibortv-mms-transcoder-itaita-2"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@cibortv-mms-transcoder-itaita-2:/opt/catramms
	date

	echo "cibortv-mms-transcoder-sat-1"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@cibortv-mms-transcoder-sat-1:/opt/catramms
	date
fi

