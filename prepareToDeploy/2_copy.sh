#!/bin/bash

date

version=$(cat ./version.txt)

currentDir=$(pwd)
moduleName=$(basename $currentDir)

#linuxName=$(cat /etc/os-release | grep "^ID=" | cut -d'=' -f2)
##linuxName using centos will be "centos", next remove "
#linuxName=$(echo $linuxName | awk '{ if (substr($0, 0, 1) == "\"") printf("%s", substr($0, 2, length($0) - 2)); else printf("%s", $0) }')

#tarFileName=$moduleName-$version-$linuxName.tar.gz
tarFileName=$moduleName-$version.tar.gz

echo -n "deploy su mms cloud/test? " 
read deploy
if [ "$deploy" == "y" ]; then
	echo "aws-mms-t-api-gui-1"
	scp -i ~/ssh-keys/aws-mms-key-ireland.pem /opt/catrasoftware/deploy/$tarFileName mms@ec2-176-34-143-242.eu-west-1.compute.amazonaws.com:/opt/catramms
	date


	echo "aws-mms-t-engine-db-1"
	scp -i ~/ssh-keys/aws-mms-key-ireland.pem /opt/catrasoftware/deploy/$tarFileName mms@ec2-52-31-108-235.eu-west-1.compute.amazonaws.com:/opt/catramms
	date

	echo "aws-mms-t-transcoder-irl-1"
	scp -i ~/ssh-keys/aws-mms-key-ireland.pem /opt/catrasoftware/deploy/$tarFileName mms@ec2-54-73-213-29.eu-west-1.compute.amazonaws.com:/opt/catramms
	date
fi

echo -n "deploy su cloud production? " 
read deploy
if [ "$deploy" == "y" ]; then
	echo "hetzner-api-gui-2"
	scp -P 9255 -i ~/ssh-keys/hetzner-mms-key.pem /opt/catrasoftware/deploy/$tarFileName mms@168.119.250.162:/opt/catramms
	date

	echo "hetzner-engine-db-2"
	scp -P 9255 -i ~/ssh-keys/hetzner-mms-key.pem /opt/catrasoftware/deploy/$tarFileName mms@168.119.60.134:/opt/catramms
	date

	echo "hetzner-transcoder-1"
	scp -P 9255 -i ~/ssh-keys/hetzner-mms-key.pem /opt/catrasoftware/deploy/$tarFileName mms@49.12.33.127:/opt/catramms
	date
fi

echo -n "deploy su cibortv? " 
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

	echo "aruba-mms-transcoder-1"
	scp -P 9255 -i ~/ssh-keys/cibortv-aruba.pem /opt/catrasoftware/deploy/$tarFileName mms@ru001940.arubabiz.net:/opt/catramms
	date

	echo "aruba-mms-transcoder-2"
	scp -P 9255 -i ~/ssh-keys/cibortv-aruba.pem /opt/catrasoftware/deploy/$tarFileName mms@ru001941.arubabiz.net:/opt/catramms
	date

fi

#make it downloadable from public
echo -n "Load package to MMSRepository-free (ubuntu 20.04)? " 
read deploy
if [ "$deploy" == "y" ]; then
	scp -i ~/ssh-keys/aws-mms-key-ireland.pem /opt/catrasoftware/deploy/$tarFileName mms@ec2-34-248-199-119.eu-west-1.compute.amazonaws.com:/var/catramms/storage/MMSRepository-free/packages
fi

echo -n "Load package to MMSRepository-free (ubuntu 22.04)? " 
read deploy
if [ "$deploy" == "y" ]; then
	scp -i ~/ssh-keys/aws-mms-key-ireland.pem /opt/catrasoftware/deploy/$tarFileName mms@ec2-34-248-199-119.eu-west-1.compute.amazonaws.com:/var/catramms/storage/MMSRepository-free/packages/ubuntu-22.04
fi

