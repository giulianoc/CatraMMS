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

echo -n "deploy su aws? " 
read deploy
if [ "$deploy" == "y" ]; then
	echo "aws-mms-api-gui-1"
	#scp -i ~/ssh-keys/aws-key-ireland.pem /opt/catrasoftware/deploy/$tarFileName mms@aws-mms-api-gui-1:/opt/catramms
	scp -i ~/ssh-keys/aws-key-ireland.pem /opt/catrasoftware/deploy/$tarFileName mms@ec2-63-35-35-24.eu-west-1.compute.amazonaws.com:/opt/catramms
	date

	echo "aws-mms-api-gui-2"
	scp -i ~/ssh-keys/aws-key-ireland.pem /opt/catrasoftware/deploy/$tarFileName mms@ec2-52-50-243-155.eu-west-1.compute.amazonaws.com:/opt/catramms
	date

	echo "aws-mms-engine-db-1"
	scp -i ~/ssh-keys/aws-key-ireland.pem /opt/catrasoftware/deploy/$tarFileName mms@ec2-34-248-199-119.eu-west-1.compute.amazonaws.com:/opt/catramms
	date

	echo "aws-mms-engine-db-2"
	scp -i ~/ssh-keys/aws-key-ireland.pem /opt/catrasoftware/deploy/$tarFileName mms@ec2-52-49-243-7.eu-west-1.compute.amazonaws.com:/opt/catramms
	date

	echo "aws-mms-transcoder-irl-1"
	scp -i ~/ssh-keys/aws-key-ireland.pem /opt/catrasoftware/deploy/$tarFileName mms@ec2-34-245-119-36.eu-west-1.compute.amazonaws.com:/opt/catramms
	date

	echo "aws-mms-transcoder-irl-2"
	scp -i ~/ssh-keys/aws-key-ireland.pem /opt/catrasoftware/deploy/$tarFileName mms@ec2-52-50-9-211.eu-west-1.compute.amazonaws.com:/opt/catramms
	date

	echo "aws-mms-transcoder-irl-3"
	scp -i ~/ssh-keys/aws-key-ireland.pem /opt/catrasoftware/deploy/$tarFileName mms@ec2-34-244-38-33.eu-west-1.compute.amazonaws.com:/opt/catramms
	date

	echo "aws-mms-transcoder-irl-4"
	scp -i ~/ssh-keys/aws-key-ireland.pem /opt/catrasoftware/deploy/$tarFileName mms@ec2-63-35-222-185.eu-west-1.compute.amazonaws.com:/opt/catramms
	date

	echo "aws-mms-transcoder-mil-1"
	scp -i ~/ssh-keys/aws-key-milan.pem /opt/catrasoftware/deploy/$tarFileName mms@ec2-15-160-110-185.eu-south-1.compute.amazonaws.com:/opt/catramms
	date

fi

