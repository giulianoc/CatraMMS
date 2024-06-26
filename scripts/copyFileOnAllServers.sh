#!/bin/bash


if [ $# -ne 2 ]
then
	echo "usage $0 <source file path name> <dest directory path name>"

	exit
fi

sourceFilePathName=$1
destDirectoryPathName=$2

#aws-api-1 aws-api-2 aws-api-3 aws-api-4 aws-engine-1 aws-engine-2 aws-engine-3 aws-engine-4 aws-transcoder-1 aws-transcoder-2 aws-transcoder-3 aws-transcoder-4
for server in ec2-63-35-35-24.eu-west-1.compute.amazonaws.com ec2-52-50-243-155.eu-west-1.compute.amazonaws.com ec2-52-48-75-149.eu-west-1.compute.amazonaws.com ec2-176-34-143-242.eu-west-1.compute.amazonaws.com ec2-34-248-199-119.eu-west-1.compute.amazonaws.com ec2-52-49-243-7.eu-west-1.compute.amazonaws.com ec2-63-34-105-227.eu-west-1.compute.amazonaws.com ec2-52-208-73-64.eu-west-1.compute.amazonaws.com ec2-18-202-38-18.eu-west-1.compute.amazonaws.com ec2-99-81-166-232.eu-west-1.compute.amazonaws.com ec2-52-18-23-12.eu-west-1.compute.amazonaws.com ec2-52-215-51-62.eu-west-1.compute.amazonaws.com
do
	echo "scp -i ~/ssh-keys/aws-mms-key-ireland.pem $sourceFilePathName mms@$server:$destDirectoryPathName"
	scp -i ~/ssh-keys/aws-mms-key-ireland.pem $sourceFilePathName mms@$server:$destDirectoryPathName
done

#cibortv-transcoder-4
echo "scp -P 9255 -i ~/ssh-keys/cibortv-transcoder-4.pem $sourceFilePathName mms@93.58.249.102:$destDirectoryPathName"
scp -P 9255 -i ~/ssh-keys/cibortv-transcoder-4.pem $sourceFilePathName mms@93.58.249.102:$destDirectoryPathName

#aruba-1 aruba-2 aruba-3
for server in ru001940.arubabiz.net ru001941.arubabiz.net ru002148.arubabiz.net
do
	echo "scp -P 9255 -i ~/ssh-keys/cibortv-aruba.pem $sourceFilePathName mms@$server:$destDirectoryPathName"
	scp -P 9255 -i ~/ssh-keys/cibortv-aruba.pem $sourceFilePathName mms@$server:$destDirectoryPathName
done

#hetzner-test-api-1 hetzner-test-engine-2 hetzner-test-engine-3 hetzner-test-transcoder-2 hetzner-api-1 hetzner-api-2 hetzner-delivery-binary-gui-5 hetzner-delivery-binary-gui-6 hetzner-engine-1 hetzner-engine-3 hetzner-transcoder-1 hetzner-transcoder-5
for server in 138.201.245.228 49.12.74.56 142.132.232.196 88.198.47.118 159.69.251.50 168.119.250.162 136.243.35.105 136.243.34.218 167.235.14.44 167.235.14.105 162.55.235.245 46.4.98.135 
do
	echo "scp -P 9255 -i ~/ssh-keys/hetzner-mms-key.pem $sourceFilePathName mms@$server:$destDirectoryPathName"
	scp -P 9255 -i ~/ssh-keys/hetzner-mms-key.pem $sourceFilePathName mms@$server:$destDirectoryPathName
done

#aws-transcoder-mil-1 aws-transcoder-mil-2
for server in ec2-15-161-78-89.eu-south-1.compute.amazonaws.com ec2-35-152-80-3.eu-south-1.compute.amazonaws.com
do
	echo "scp -i ~/ssh-keys/aws-cibortv1-key-milan.pem $sourceFilePathName mms@$server:$destDirectoryPathName"
	scp -i ~/ssh-keys/aws-cibortv1-key-milan.pem $sourceFilePathName mms@$server:$destDirectoryPathName
done


#aws-integration-1 aws-integration-2 aws-integration-3 aws-integration-4
for server in ec2-54-76-8-245.eu-west-1.compute.amazonaws.com ec2-18-202-82-214.eu-west-1.compute.amazonaws.com ec2-54-78-165-54.eu-west-1.compute.amazonaws.com ec2-63-34-124-54.eu-west-1.compute.amazonaws.com
do
	echo "scp -i ~/ssh-keys/aws-hdea-key-integration-ireland.pem $sourceFilePathName mms@$server:$destDirectoryPathName"
	scp -i ~/ssh-keys/aws-hdea-key-integration-ireland.pem $sourceFilePathName mms@$server:$destDirectoryPathName
done

