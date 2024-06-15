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
	echo "hetzner-test-api-1"
	scp -P 9255 -i ~/ssh-keys/hetzner-mms-key.pem /opt/catrasoftware/deploy/$tarFileName mms@138.201.245.228:/opt/catramms
	date

	echo "hetzner-test-engine-db-2"
	scp -P 9255 -i ~/ssh-keys/hetzner-mms-key.pem /opt/catrasoftware/deploy/$tarFileName mms@49.12.74.56:/opt/catramms
	date

	echo "hetzner-test-engine-db-3"
	scp -P 9255 -i ~/ssh-keys/hetzner-mms-key.pem /opt/catrasoftware/deploy/$tarFileName mms@142.132.232.196:/opt/catramms
	date

	echo "hetzner-test-transcoder-2"
	scp -P 9255 -i ~/ssh-keys/hetzner-mms-key.pem /opt/catrasoftware/deploy/$tarFileName mms@88.198.47.118:/opt/catramms
	date

fi

echo ""
echo -n "deploy su cloud production (rel 22)? " 
read deploy
if [ "$deploy" == "y" ]; then
	echo "hetzner-api-1"
	scp -P 9255 -i ~/ssh-keys/hetzner-mms-key.pem /opt/catrasoftware/deploy/$tarFileName mms@159.69.251.50:/opt/catramms
	date

	echo "hetzner-api-2"
	scp -P 9255 -i ~/ssh-keys/hetzner-mms-key.pem /opt/catrasoftware/deploy/$tarFileName mms@168.119.250.162:/opt/catramms
	date

	echo "hetzner-delivery-binary-gui-1"
	scp -P 9255 -i ~/ssh-keys/hetzner-mms-key.pem /opt/catrasoftware/deploy/$tarFileName mms@5.9.57.85:/opt/catramms
	date

	echo "hetzner-delivery-binary-gui-1"
	scp -P 9255 -i ~/ssh-keys/hetzner-mms-key.pem /opt/catrasoftware/deploy/$tarFileName mms@5.9.57.85:/opt/catramms
	date

	echo "hetzner-delivery-binary-gui-5"
	scp -P 9255 -i ~/ssh-keys/hetzner-mms-key.pem /opt/catrasoftware/deploy/$tarFileName mms@136.243.35.105:/opt/catramms
	date

	echo "hetzner-engine-db-1"
	scp -P 9255 -i ~/ssh-keys/hetzner-mms-key.pem /opt/catrasoftware/deploy/$tarFileName mms@167.235.14.44:/opt/catramms
	date

	echo "hetzner-engine-db-3"
	scp -P 9255 -i ~/ssh-keys/hetzner-mms-key.pem /opt/catrasoftware/deploy/$tarFileName mms@167.235.14.105:/opt/catramms
	date

	echo "cibortv-transcoder-4"
	scp -P 9255 -i ~/ssh-keys/cibortv-transcoder-4.pem /opt/catrasoftware/deploy/$tarFileName mms@93.58.249.102:/opt/catramms
	date

	echo "hetzner-transcoder-1"
	scp -P 9255 -i ~/ssh-keys/hetzner-mms-key.pem /opt/catrasoftware/deploy/$tarFileName mms@162.55.235.245:/opt/catramms
	date

	echo "hetzner-transcoder-2"
	scp -P 9255 -i ~/ssh-keys/hetzner-mms-key.pem /opt/catrasoftware/deploy/$tarFileName mms@136.243.34.218:/opt/catramms
	date

	echo "hetzner-transcoder-5"
	scp -P 9255 -i ~/ssh-keys/hetzner-mms-key.pem /opt/catrasoftware/deploy/$tarFileName mms@46.4.98.135:/opt/catramms
	date

	echo "aws-cibortv-transcoder-mil-1"
	scp -i ~/ssh-keys/aws-cibortv1-key-milan.pem /opt/catrasoftware/deploy/$tarFileName mms@ec2-15-161-78-89.eu-south-1.compute.amazonaws.com:/opt/catramms
	date

	echo "aws-cibortv-transcoder-mil-2"
	scp -i ~/ssh-keys/aws-cibortv1-key-milan.pem /opt/catrasoftware/deploy/$tarFileName mms@ec2-35-152-80-3.eu-south-1.compute.amazonaws.com:/opt/catramms
	date

	echo "aruba-cibortv-transcoder-1"
	scp -P 9255 -i ~/ssh-keys/cibortv-aruba.pem /opt/catrasoftware/deploy/$tarFileName mms@ru001940.arubabiz.net:/opt/catramms
	date

	echo "aruba-mms-transcoder-2"
	scp -P 9255 -i ~/ssh-keys/cibortv-aruba.pem /opt/catrasoftware/deploy/$tarFileName mms@ru001941.arubabiz.net:/opt/catramms
	date

	echo "aruba-mms-transcoder-3"
	scp -P 9255 -i ~/ssh-keys/cibortv-aruba.pem /opt/catrasoftware/deploy/$tarFileName mms@ru002148.arubabiz.net:/opt/catramms
	date

	echo "aws-mms-api-gui-1"
	scp -i ~/ssh-keys/aws-mms-key-ireland.pem /opt/catrasoftware/deploy/$tarFileName mms@ec2-63-35-35-24.eu-west-1.compute.amazonaws.com:/opt/catramms
	date

	echo "aws-mms-api-gui-2"
	scp -i ~/ssh-keys/aws-mms-key-ireland.pem /opt/catrasoftware/deploy/$tarFileName mms@ec2-52-50-243-155.eu-west-1.compute.amazonaws.com:/opt/catramms
	date

	echo "aws-mms-api-gui-3"
	scp -i ~/ssh-keys/aws-mms-key-ireland.pem /opt/catrasoftware/deploy/$tarFileName mms@ec2-52-48-75-149.eu-west-1.compute.amazonaws.com:/opt/catramms
	date

	echo "aws-mms-api-gui-4"
	scp -i ~/ssh-keys/aws-mms-key-ireland.pem /opt/catrasoftware/deploy/$tarFileName mms@ec2-176-34-143-242.eu-west-1.compute.amazonaws.com:/opt/catramms
	date

	echo "aws-mms-engine-db-1"
	scp -i ~/ssh-keys/aws-mms-key-ireland.pem /opt/catrasoftware/deploy/$tarFileName mms@ec2-34-248-199-119.eu-west-1.compute.amazonaws.com:/opt/catramms
	date

	echo "aws-mms-engine-db-2"
	scp -i ~/ssh-keys/aws-mms-key-ireland.pem /opt/catrasoftware/deploy/$tarFileName mms@ec2-52-49-243-7.eu-west-1.compute.amazonaws.com:/opt/catramms
	date

	echo "aws-mms-engine-db-3"
	scp -i ~/ssh-keys/aws-mms-key-ireland.pem /opt/catrasoftware/deploy/$tarFileName mms@ec2-63-34-105-227.eu-west-1.compute.amazonaws.com:/opt/catramms
	date

	echo "aws-mms-engine-db-4"
	scp -i ~/ssh-keys/aws-mms-key-ireland.pem /opt/catrasoftware/deploy/$tarFileName mms@ec2-52-208-73-64.eu-west-1.compute.amazonaws.com:/opt/catramms
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

fi

echo ""
echo -n "deploy su cibortv (rel 20)? " 
read deploy
if [ "$deploy" == "y" ]; then
	echo "aruba-mms-transcoder-1"
	scp -P 9255 -i ~/ssh-keys/cibortv-aruba.pem /opt/catrasoftware/deploy/$tarFileName mms@ru001940.arubabiz.net:/opt/catramms
	date
fi

#make it downloadable from public
echo -n "Load package to MMSRepository-free (ubuntu 20.04)? " 
read deploy
if [ "$deploy" == "y" ]; then
	scp -i ~/ssh-keys/aws-mms-key-ireland.pem /opt/catrasoftware/deploy/$tarFileName mms@ec2-34-248-199-119.eu-west-1.compute.amazonaws.com:/var/catramms/storage/MMSRepository-free/packages/ubuntu-20.04
fi

echo -n "Load package to MMSRepository-free (ubuntu 22.04)? " 
read deploy
if [ "$deploy" == "y" ]; then
	scp -P 9255 -i ~/ssh-keys/hetzner-mms-key.pem /opt/catrasoftware/deploy/$tarFileName mms@168.119.250.162:/var/catramms/storage/MMSRepository-free/packages/ubuntu-22.04
fi

