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
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@176.31.140.131:/opt/catramms
	date

	echo "mms-t-engine-db-1"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@51.210.161.250:/opt/catramms
	date

	echo "mms-t-transcoder-fr-1"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@51.210.161.249:/opt/catramms
	date
fi

echo -n "deploy su cibor? " 
read deploy
if [ "$deploy" == "y" ]; then
	echo "cibortv-mms-api-gui-1"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@164.132.248.228:/opt/catramms
	date

	echo "cibortv-mms-api-gui-2"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@164.132.248.229:/opt/catramms
	date

	echo "cibortv-mms-engine-db-1"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@164.132.248.235:/opt/catramms
	date

	echo "cibortv-mms-engine-db-2"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@51.210.161.241:/opt/catramms
	date

	echo "cibortv-mms-engine-db-3"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@176.31.140.132:/opt/catramms
	date

	echo "cibortv-mms-transcoder-fr-1"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@164.132.248.225:/opt/catramms
	date

	echo "cibortv-mms-transcoder-fr-2"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@164.132.248.231:/opt/catramms
	date

	echo "cibortv-mms-transcoder-fr-3"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@164.132.248.232:/opt/catramms
	date

	echo "cibortv-mms-transcoder-fr-4"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@51.210.161.242:/opt/catramms
	date

	echo "cibortv-mms-transcoder-fr-11"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@135.125.97.201:/opt/catramms
	date

	echo "cibortv-mms-transcoder-fr-12"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@152.228.220.72:/opt/catramms
	date

	echo "cibortv-mms-transcoder-ita-1"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@176.31.140.129:/opt/catramms
	date

	echo "cibortv-mms-transcoder-ita-2"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@176.31.140.130:/opt/catramms
	date

	echo "cibortv-mms-transcoder-itaita-1"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@188.213.169.34:/opt/catramms
	date

	echo "cibortv-mms-transcoder-itaita-2"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@95.110.132.155:/opt/catramms
	date

	echo "cibortv-mms-transcoder-sat-1"
	scp -P 9255 /opt/catrasoftware/deploy/$tarFileName mms@192.168.0.200:/opt/catramms
	date
fi

echo -n "deploy su aws? " 
read deploy
if [ "$deploy" == "y" ]; then
	echo "aws-mms-api-gui-1"
	#scp -i ~/ssh-keys/aws-key-ireland.pem /opt/catrasoftware/deploy/$tarFileName mms@aws-mms-api-gui-1:/opt/catramms
	scp -i ~/ssh-keys/aws-mms-key-ireland.pem /opt/catrasoftware/deploy/$tarFileName mms@ec2-63-35-35-24.eu-west-1.compute.amazonaws.com:/opt/catramms
	date

	echo "aws-mms-api-gui-2"
	scp -i ~/ssh-keys/aws-mms-key-ireland.pem /opt/catrasoftware/deploy/$tarFileName mms@ec2-52-50-243-155.eu-west-1.compute.amazonaws.com:/opt/catramms
	date

	echo "aws-mms-engine-db-1"
	scp -i ~/ssh-keys/aws-mms-key-ireland.pem /opt/catrasoftware/deploy/$tarFileName mms@ec2-34-248-199-119.eu-west-1.compute.amazonaws.com:/opt/catramms
	date

	echo "aws-mms-engine-db-2"
	scp -i ~/ssh-keys/aws-mms-key-ireland.pem /opt/catrasoftware/deploy/$tarFileName mms@ec2-52-49-243-7.eu-west-1.compute.amazonaws.com:/opt/catramms
	date

	echo "aws-mms-transcoder-irl-1"
	scp -i ~/ssh-keys/aws-mms-key-ireland.pem /opt/catrasoftware/deploy/$tarFileName mms@ec2-18-202-38-18.eu-west-1.compute.amazonaws.com:/opt/catramms
	date

	echo "aws-mms-transcoder-irl-2"
	scp -i ~/ssh-keys/aws-mms-key-ireland.pem /opt/catrasoftware/deploy/$tarFileName mms@ec2-99-81-166-232.eu-west-1.compute.amazonaws.com:/opt/catramms
	date

	echo "aws-mms-transcoder-irl-3"
	scp -i ~/ssh-keys/aws-mms-key-ireland.pem /opt/catrasoftware/deploy/$tarFileName mms@ec2-52-18-23-12.eu-west-1.compute.amazonaws.com:/opt/catramms
	date

	echo "aws-mms-transcoder-irl-4"
	scp -i ~/ssh-keys/aws-mms-key-ireland.pem /opt/catrasoftware/deploy/$tarFileName mms@ec2-52-215-51-62.eu-west-1.compute.amazonaws.com:/opt/catramms
	date

	echo "aws-mms-transcoder-mil-1"
	scp -i ~/ssh-keys/aws-cibortv1-key-milan.pem /opt/catrasoftware/deploy/$tarFileName mms@ec2-15-161-78-89.eu-south-1.compute.amazonaws.com:/opt/catramms
	date

	echo "aws-mms-transcoder-mil-2"
	scp -i ~/ssh-keys/aws-cibortv1-key-milan.pem /opt/catrasoftware/deploy/$tarFileName mms@ec2-35-152-80-3.eu-south-1.compute.amazonaws.com:/opt/catramms
	date

fi

