#!/bin/bash

date

version=$(cat ./version.txt)

currentDir=$(pwd)
moduleName=$(basename $currentDir)

linuxName=$(cat /etc/os-release | grep "^ID=" | cut -d'=' -f2)
#linuxName using centos will be "centos", next remove "
linuxName=$(echo $linuxName | awk '{ if (substr($0, 0, 1) == "\"") printf("%s", substr($0, 2, length($0) - 2)); else printf("%s", $0) }')

tarFileName=$moduleName-$version-$linuxName.tar.gz


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

	echo "192.168.0.121"
	scp /opt/catrasoftware/deploy/$tarFileName mms@192.168.0.121:/opt/catramms
	date
fi

echo -n "deploy su rsis-lab-mmst? " 
read deploy
if [ "$deploy" == "y" ]; then
	echo "rsis-lab-mmst"
	scp /opt/catrasoftware/deploy/$tarFileName mms@rsis-lab-mmst:/opt/catramms
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

	echo "cibortv-mms-transcoder-es-1"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@cibortv-mms-transcoder-es-1:/opt/catramms
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

	echo "cibortv-mms-transcoder-fr-5"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@cibortv-mms-transcoder-fr-5:/opt/catramms
	date

	echo "cibortv-mms-transcoder-fr-6"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@cibortv-mms-transcoder-fr-6:/opt/catramms
	date

	echo "cibortv-mms-transcoder-fr-7"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@cibortv-mms-transcoder-fr-7:/opt/catramms
	date

	echo "cibortv-mms-transcoder-fr-8"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@cibortv-mms-transcoder-fr-8:/opt/catramms
	date

	echo "cibortv-mms-transcoder-fr-9"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@cibortv-mms-transcoder-fr-9:/opt/catramms
	date

	echo "cibortv-mms-transcoder-fr-10"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@cibortv-mms-transcoder-fr-10:/opt/catramms
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

#echo -n "deploy su mms icml? " 
#read deploy
#if [ "$deploy" == "y" ]; then
#	echo "icml-1"
#	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@icml-1:/opt/catramms
#	date

#	echo "icml-2"
#	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@icml-2:/opt/catramms
#	date

#fi

